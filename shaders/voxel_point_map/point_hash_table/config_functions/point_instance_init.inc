#ifndef POINT_HASH_TABLE_POINT_INSTANCE_INIT
#define POINT_HASH_TABLE_POINT_INSTANCE_INIT

PointInstance point_instance_init(uvec2 key, out bool success) {
    success = true;
    return PointInstance(vec4(vec2(key), 0.0, 1.0), vec4(1.0));
}

#endif
