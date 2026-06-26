#pragma once
#include "vao.h"
#include "vbo.h"
#include "ebo.h"

#include <glm/glm.hpp>
#include <vector>
#include <cstddef> // offsetof

#include "drawable.h"
#include "transformable.h"

struct alignas(16) PointInstance {
    glm::vec4 pos;
    glm::vec4 color;  // per-point color (RGB 0..1)
    
    // float time;
    // glm::vec3 gps_pos;
    // glm::vec3 imu_rotation;
};

class Point : public Drawable, public Transformable {
public:
    Point();
    Point(SSBO& instance_ssbo, int instance_count);
    ~Point();

    float size_px = 2.0f;
    float size_world = 0.05f;

    bool constant_screen_size = true;
    bool round = true;

    void set_points(const std::vector<PointInstance>& points);
    void set_points(SSBO& points, int num_points);
    void draw(RenderState state) override;
    VBO* instance_vbo = nullptr;   // dynamic: pos/color per point

    int instance_count = 0;
private:
    VAO* vao = nullptr;
    VBO* quad_vbo = nullptr;       // static: 4 corners
    EBO* quad_ebo = nullptr;       // static: 6 indices
    
    
};
