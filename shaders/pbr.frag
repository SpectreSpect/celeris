#version 450

#define USE_CLUSTERED_LIGHTS 1

layout(location = 0) in vec3 frag_world_pos;
layout(location = 1) in vec3 frag_normal;
layout(location = 2) in vec2 frag_uv;
layout(location = 3) in vec3 frag_tangent;
layout(location = 4) in float frag_tangent_sign;
layout(location = 5) in vec3 frag_view_pos;

layout(location = 0) out vec4 out_color;

struct MaterialData {
    // x = metallic, y = roughness, z = ambient occlusion, w = exposure
    vec4 material;

    // rgb = albedo/base color, a = environment multiplier
    vec4 color;

    // x = irradiance cubemap id, y = prefilter cubemap id
    uvec4 pbr_map_ids;
};

struct LightSource {
    vec4 position; // xyz = position, w = radius
    vec4 color;    // xyz = color,    w = intensity
};

layout(std430, set = 0, binding = 0) readonly buffer MaterialBuffer {
    MaterialData materials[];
} material_buffer;

layout(set = 0, binding = 1) uniform samplerCubeArray irradianceMaps;
layout(set = 0, binding = 2) uniform samplerCubeArray prefilterMaps;
layout(set = 0, binding = 3) uniform sampler2D brdfLUT;

layout(set = 1, binding = 0) uniform CameraUniform {
    mat4 view;
    mat4 proj;
    vec4 viewPos;
} camera_uniform;

#if USE_CLUSTERED_LIGHTS
layout(set = 1, binding = 3) uniform ClusteredLightingUniform {
    uvec4 cluster_grid;
    vec4 screen_params;
} lighting_uniform;

layout(std430, set = 1, binding = 5) readonly buffer LightSourcesBuffer {
    LightSource light_sources[];
};

layout(std430, set = 1, binding = 6) readonly buffer NumLightsInClustersBuffer {
    uint num_lights_in_clusters[];
};

layout(std430, set = 1, binding = 7) readonly buffer LightsInClustersBuffer {
    uint lights_in_clusters[];
};
#endif

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint material_data_id;
} pc;

const float PI = 3.14159265359;

vec3 linear_to_srgb(vec3 color)
{
    color = max(color, vec3(0.0));
    return pow(color, vec3(1.0 / 2.2));
}

vec3 tonemap_reinhard(vec3 color)
{
    return color / (color + vec3(1.0));
}

#if USE_CLUSTERED_LIGHTS
uint compute_cluster_id()
{
    float screen_width  = lighting_uniform.screen_params.x;
    float screen_height = lighting_uniform.screen_params.y;
    float near_plane    = lighting_uniform.screen_params.z;
    float far_plane     = lighting_uniform.screen_params.w;

    uint x_tiles  = lighting_uniform.cluster_grid.x;
    uint y_tiles  = lighting_uniform.cluster_grid.y;
    uint z_slices = lighting_uniform.cluster_grid.z;

    float fx = clamp(gl_FragCoord.x, 0.0, screen_width - 1.0);
    float fy = clamp(gl_FragCoord.y, 0.0, screen_height - 1.0);

    // Important: match CPU cluster generation where y=0 is bottom.
    fy = screen_height - 1.0 - fy;

    uint tile_x = uint(fx * float(x_tiles) / screen_width);
    uint tile_y = uint(fy * float(y_tiles) / screen_height);

    tile_x = min(tile_x, x_tiles - 1u);
    tile_y = min(tile_y, y_tiles - 1u);

    float view_z = -frag_view_pos.z;
    view_z = max(view_z, near_plane);

    float ln_z = log(max(view_z, 1e-6));
    float ln_near = log(max(near_plane, 1e-6));
    float ln_far = log(max(far_plane, near_plane + 1e-6));

    float slice_t = (ln_z - ln_near) / max(ln_far - ln_near, 1e-6);
    slice_t = clamp(slice_t, 0.0, 0.999999);

    uint z_slice = uint(floor(slice_t * float(z_slices)));
    z_slice = min(z_slice, z_slices - 1u);

    return tile_x + tile_y * x_tiles + z_slice * x_tiles * y_tiles;
}
#endif

float distribution_ggx(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 1e-6);
}

float geometry_schlick_ggx(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    float denom = NdotV * (1.0 - k) + k;
    return NdotV / max(denom, 1e-6);
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    float ggxV = geometry_schlick_ggx(NdotV, roughness);
    float ggxL = geometry_schlick_ggx(NdotL, roughness);

    return ggxV * ggxL;
}

vec3 fresnel_schlick(float cos_theta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

vec3 fresnel_schlick_roughness(float cos_theta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) *
                pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

void accumulate_pbr_light(
    vec3 N,
    vec3 V,
    vec3 albedo,
    float metallic,
    float roughness,
    vec3 F0,
    vec3 L,
    vec3 radiance,
    inout vec3 Lo)
{
    vec3 H = normalize(V + L);

    float NDF = distribution_ggx(N, H, roughness);
    float G = geometry_smith(N, V, L, roughness);
    vec3 F = fresnel_schlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    vec3 numerator = NDF * G * F;
    float denominator = max(4.0 * NdotV * NdotL, 1e-6);
    vec3 specular = numerator / denominator;

    Lo += (kD * albedo / PI + specular) * radiance * NdotL;
}

void main()
{
    MaterialData material_data = material_buffer.materials[pc.material_data_id];
    float irradiance_map_id = float(material_data.pbr_map_ids.x);
    float prefilter_map_id = float(material_data.pbr_map_ids.y);

    vec3 albedo = clamp(material_data.color.rgb, 0.0, 1.0);
    float environment_multiplier = material_data.color.a > 0.0
        ? material_data.color.a
        : 1.0;

    float metallic = clamp(material_data.material.x, 0.0, 1.0);
    float roughness = material_data.material.y > 0.0
        ? material_data.material.y
        : 1.0;
    roughness = clamp(roughness, 0.0005, 1.0);

    float ao = material_data.material.z > 0.0
        ? material_data.material.z
        : 1.0;
    ao = clamp(ao, 0.0, 1.0);

    float exposure = material_data.material.w > 0.0
        ? material_data.material.w
        : 1.0;

    vec3 N = normalize(frag_normal);

    // Derive the camera world position from the view matrix instead of relying
    // on viewPos being filled correctly. A bad/stale viewPos makes metallic
    // reflections form a dark circular artifact around the camera.
    vec3 camera_world_pos = inverse(camera_uniform.view)[3].xyz;

    // V points from the fragment toward the camera. For reflect(), the incident
    // vector must point from the camera toward the fragment, so use -V.
    vec3 V = normalize(camera_world_pos - frag_world_pos);
    vec3 R = normalize(reflect(-V, N));

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);

#if USE_CLUSTERED_LIGHTS
    uint max_lights_per_cluster = lighting_uniform.cluster_grid.w;

    uint cluster_id = compute_cluster_id();
    uint cluster_base = cluster_id * max_lights_per_cluster;

    uint count = min(
        num_lights_in_clusters[cluster_id],
        max_lights_per_cluster
    );

    for (uint i = 0u; i < count; ++i) {
        uint light_index = lights_in_clusters[cluster_base + i];
        LightSource light_source = light_sources[light_index];

        vec3 light_vec = light_source.position.xyz - frag_world_pos;
        float dist_to_light = length(light_vec);
        vec3 L = light_vec / max(dist_to_light, 1e-6);

        float radius = light_source.position.w;
        float x = clamp(dist_to_light / max(radius, 1e-6), 0.0, 1.0);
        float fade = 1.0 - smoothstep(0.0, 1.0, x);
        float attenuation = fade / max(dist_to_light * dist_to_light, 1e-4);

        vec3 radiance =
            light_source.color.rgb *
            light_source.color.w *
            attenuation *
            (radius * radius);

        accumulate_pbr_light(
            N,
            V,
            albedo,
            metallic,
            roughness,
            F0,
            L,
            radiance,
            Lo
        );
    }
#else
    vec3 L = normalize(vec3(1.0, 1.5, 1.3));
    vec3 radiance = vec3(0.0);

    accumulate_pbr_light(
        N,
        V,
        albedo,
        metallic,
        roughness,
        F0,
        L,
        radiance,
        Lo
    );
#endif

    float NdotV = max(dot(N, V), 0.0);

    vec3 F = fresnel_schlick_roughness(NdotV, F0, roughness);
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 irradiance = texture(irradianceMaps, vec4(N, irradiance_map_id)).rgb * environment_multiplier;
    vec3 diffuse_ibl = irradiance * albedo;

    int prefilter_levels = textureQueryLevels(prefilterMaps);
    float prefilter_max_mip = max(float(prefilter_levels - 1), 0.0);
    float lod = roughness * prefilter_max_mip;

    vec3 prefiltered_color = textureLod(
        prefilterMaps,
        vec4(normalize(R), prefilter_map_id),
        lod
    ).rgb * environment_multiplier;

    vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3 specular_ibl = prefiltered_color * (F * brdf.x + brdf.y);

    vec3 ambient = (kD * diffuse_ibl + specular_ibl) * ao;

    vec3 color = ambient + Lo;
    color *= exposure;
    color = tonemap_reinhard(color);
    color = linear_to_srgb(color);

    out_color = vec4(color, 1.0);
}
