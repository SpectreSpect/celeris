#version 460

layout(location = 0) in vec2 aCorner;

layout(location = 0) out vec4 vColor;

struct PointInstance {
    vec4 position;
    vec4 color;
};

layout(std140, set = 0, binding = 0) uniform NormalCloudUniformBuffer {
    mat4 view;
    mat4 proj;
    vec2 viewport;
} ubo;

layout(std430, set = 0, binding = 1) readonly buffer PointInstanceBuffer {
    PointInstance points[];
} point_buffer;

layout(std430, set = 0, binding = 2) readonly buffer NormalBuffer {
    vec4 normals[];
} normal_buffer;

layout(push_constant) uniform NormalCloudPushConstants {
    vec4 color;
    mat4 model;

    float normal_length;
    float line_width_px;
    int use_point_color;
    int pad_0;
} pc;


void main() {
    uint id = gl_InstanceIndex;

    PointInstance point = point_buffer.points[id];
    vec4 normal4 = normal_buffer.normals[id];

    vec3 localPoint = point.position.xyz;
    vec3 localNormal = normal4.xyz;

    float normalLen = length(localNormal);

    bool valid =
        point.color.a > 0.001 &&
        normalLen > 0.00001;

    if (!valid) {
        vColor = vec4(0.0);
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    localNormal /= normalLen;

    if (pc.use_point_color == 1) {
        vColor = point.color;
    } else {
        vColor = pc.color;
    }

    vec3 worldStart = (pc.model * vec4(localPoint, 1.0)).xyz;

    mat3 normalMatrix = transpose(inverse(mat3(pc.model)));
    vec3 worldNormal = normalize(normalMatrix * localNormal);

    vec3 worldEnd = worldStart + worldNormal * pc.normal_length;

    vec4 startClip = ubo.proj * ubo.view * vec4(worldStart, 1.0);
    vec4 endClip   = ubo.proj * ubo.view * vec4(worldEnd, 1.0);

    if (startClip.w <= 0.0001 || endClip.w <= 0.0001) {
        vColor = vec4(0.0);
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec2 startNdc = startClip.xy / startClip.w;
    vec2 endNdc   = endClip.xy / endClip.w;

    vec2 startPx = (startNdc * 0.5 + 0.5) * ubo.viewport;
    vec2 endPx   = (endNdc   * 0.5 + 0.5) * ubo.viewport;

    vec2 dirPx = endPx - startPx;

    if (length(dirPx) < 0.0001) {
        dirPx = vec2(1.0, 0.0);
    } else {
        dirPx = normalize(dirPx);
    }

    vec2 perpPx = vec2(-dirPx.y, dirPx.x);

    float side = aCorner.x;
    float lineT = aCorner.y;

    vec2 basePx = mix(startPx, endPx, lineT);
    vec2 finalPx = basePx + perpPx * side * 0.5 * pc.line_width_px;

    vec2 finalNdc = finalPx / ubo.viewport * 2.0 - 1.0;

    vec4 baseClip = mix(startClip, endClip, lineT);

    gl_Position = vec4(
        finalNdc * baseClip.w,
        baseClip.z,
        baseClip.w
    );
}