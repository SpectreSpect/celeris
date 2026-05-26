#version 450

#define USE_CLUSTERED_LIGHTS 0

layout(location = 0) in vec3 frag_world_pos;
layout(location = 1) in vec3 frag_normal;
layout(location = 2) in vec2 frag_uv;

layout(location = 0) out vec4 out_color;

struct MaterialData {
    vec4 material;
    vec4 color;
};

struct LightSource {
    vec4 position; // xyz = position, w = radius
    vec4 color;    // xyz = color,    w = intensity
};

layout(std430, set = 0, binding = 0) readonly buffer MaterialBuffer {
    MaterialData materials[];
} material_buffer;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

#if USE_CLUSTERED_LIGHTS
layout(std430, set = 0, binding = 4) readonly buffer LightSourcesBuffer {
    LightSource light_sources[];
};

layout(std430, set = 0, binding = 5) readonly buffer NumLightsInClustersBuffer {
    uint num_lights_in_clusters[];
};

layout(std430, set = 0, binding = 6) readonly buffer LightsInClustersBuffer {
    uint lights_in_clusters[];
};
#endif

layout(set = 1, binding = 0) uniform CameraUniform {
    mat4 view;
    mat4 proj;
    vec4 viewPos;
} camera_uniform;

layout(set = 1, binding = 1) uniform ClusteredLightingUniform {
    // x = x tiles
    // y = y tiles
    // z = z slices
    // w = max lights per cluster
    uvec4 cluster_grid;

    // x = screen width
    // y = screen height
    // z = near plane
    // w = far plane
    vec4 screen_params;
} lighting_uniform;

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint material_data_id;
} pc;

vec4 linear_to_srgb(vec4 color)
{
    vec3 rgb = max(color.rgb, vec3(0.0));
    return vec4(pow(rgb, vec3(1.0 / 2.2)), color.a);
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

    uint tile_x = uint(fx * float(x_tiles) / screen_width);
    uint tile_y = uint(fy * float(y_tiles) / screen_height);

    tile_x = min(tile_x, x_tiles - 1u);
    tile_y = min(tile_y, y_tiles - 1u);

    vec3 frag_view_pos = vec3(camera_uniform.view * vec4(frag_world_pos, 1.0));

    float view_z = -frag_view_pos.z;
    if (view_z <= 0.0) {
        view_z = near_plane;
    }

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

void accumulate_blinn_phong_light(
    vec3 N,
    vec3 V,
    vec3 base_color,
    float diffuse_strength,
    float specular_strength,
    float shininess,
    vec3 L,
    vec3 radiance,
    inout vec3 result
) {
    vec3 H = normalize(L + V);

    float diffuse = max(dot(N, L), 0.0);

    float specular = 0.0;
    if (diffuse > 0.0) {
        specular = pow(max(dot(N, H), 0.0), shininess);
    }

    vec3 diffuse_color = diffuse_strength * diffuse * base_color * radiance;
    vec3 specular_color = specular_strength * specular * radiance;

    result += diffuse_color + specular_color;
}

void main()
{
    MaterialData material_data = material_buffer.materials[pc.material_data_id];

    vec3 base_color = texture(texSampler, frag_uv).rgb * material_data.color.rgb;

    vec4 material = material_data.material;

    float ambient_strength  = material.x;
    float diffuse_strength  = material.y;
    float specular_strength = material.z;
    float shininess         = max(material.w, 1.0);

    vec3 N = normalize(frag_normal);
    vec3 V = normalize(camera_uniform.viewPos.xyz - frag_world_pos);

    vec3 final_color = ambient_strength * base_color;

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
        float x = clamp(dist_to_light / radius, 0.0, 1.0);

        float fade = 1.0 - smoothstep(0.0, 1.0, x);
        float attenuation = fade / max(dist_to_light * dist_to_light, 1e-4);

        vec3 radiance =
            light_source.color.rgb *
            light_source.color.w *
            attenuation *
            (radius * radius);

        accumulate_blinn_phong_light(
            N,
            V,
            base_color,
            diffuse_strength,
            specular_strength,
            shininess,
            L,
            radiance,
            final_color
        );
    }
#else
    vec3 L = normalize(vec3(-1.0, 0.5, -1.0));
    vec3 radiance = vec3(1.0);

    accumulate_blinn_phong_light(
        N,
        V,
        base_color,
        diffuse_strength,
        specular_strength,
        shininess,
        L,
        radiance,
        final_color
    );
#endif

    out_color = linear_to_srgb(vec4(final_color, 1.0));
}