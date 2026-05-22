#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <array>
#include <vector>


struct Plane {
    glm::vec3 n; // normal
    float d;     // offset

    // signed distance from point to plane
    float distance(const glm::vec3& p) const {
        return glm::dot(n, p) + d;
    }

    void normalize() {
        float len = glm::length(n);
        if (len > 0.0f) { n /= len; d /= len; }
    }
};

struct AABB {
    glm::vec4 min;
    glm::vec4 max;   
};


class Camera{
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    float near = 0.1f;
    // float far = 5000.0f;
    float far = 1000.0f;
    float fov;
    // float movment_speed;
        
    float yaw = -90.0f;
    float pitch = 0.0f;
    // float lastX;
    // float lastY;
    bool firstMouse = true;

    std::array<Plane, 6> frustum_planes;

    Camera(glm::vec3 pos = {0.0f, 1.0f, 5.0f}, glm::vec3 up_vec = {0.0f, 1.0f, 0.0f}, float fov_deg = 60.0f);

    glm::mat4 get_view_matrix() const;
    glm::mat4 get_projection_matrix(float aspect_ratio) const;

    void update_frustum_planes(const glm::mat4& VP) {
        glm::vec4 r0 = glm::row(VP, 0);
        glm::vec4 r1 = glm::row(VP, 1);
        glm::vec4 r2 = glm::row(VP, 2);
        glm::vec4 r3 = glm::row(VP, 3);

        auto make_plane = [](const glm::vec4& v) {
            Plane pl;
            pl.n = glm::vec3(v.x, v.y, v.z);
            pl.d = v.w;
            pl.normalize();
            return pl;
        };

        frustum_planes[0] = make_plane(r3 + r0); // left
        frustum_planes[1] = make_plane(r3 - r0); // right
        frustum_planes[2] = make_plane(r3 + r1); // bottom
        frustum_planes[3] = make_plane(r3 - r1); // top
        frustum_planes[4] = make_plane(r3 + r2); // near
        frustum_planes[5] = make_plane(r3 - r2); // far
    }

    bool visible_AABB(const glm::vec3& bmin, const glm::vec3& bmax) const{
        for (const Plane& pl : frustum_planes) {
            // “positive vertex” (support point) in direction of plane normal
            glm::vec3 v(
                pl.n.x >= 0.0f ? bmax.x : bmin.x,
                pl.n.y >= 0.0f ? bmax.y : bmin.y,
                pl.n.z >= 0.0f ? bmax.z : bmin.z
            );
    
            if (pl.distance(v) < 0.0f) return false;
        }
        return true;
    }
};