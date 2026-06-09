#include "sphere.h"

Sphere::Sphere() {
    constexpr float PI = 3.14159265358979323846f;

    float radius = 1.0f;
    int sectors = 64;
    int stacks  = 32;

    struct SphereBuildVertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec3 color;
    };

    std::vector<SphereBuildVertex> temp_vertices;
    std::vector<unsigned int> sphere_indices;

    temp_vertices.reserve((stacks + 1) * (sectors + 1));
    sphere_indices.reserve(stacks * sectors * 6);

    // ------------------------------------------------------------
    // Build sphere vertices
    // ------------------------------------------------------------
    for (int i = 0; i <= stacks; ++i) {
        float stackFrac  = float(i) / float(stacks);
        float stackAngle = PI * 0.5f - stackFrac * PI; // +pi/2 -> -pi/2

        float xy = std::cos(stackAngle);
        float y  = std::sin(stackAngle);

        for (int j = 0; j <= sectors; ++j) {
            float sectorFrac  = float(j) / float(sectors);
            float sectorAngle = sectorFrac * 2.0f * PI;

            float x = xy * std::cos(sectorAngle);
            float z = xy * std::sin(sectorAngle);

            SphereBuildVertex vert;
            vert.pos    = glm::vec3(radius * x, radius * y, radius * z);
            vert.normal = glm::normalize(glm::vec3(x, y, z));
            vert.color  = glm::vec3(0.564705882f, 0.835294118f, 1.0f);

            temp_vertices.push_back(vert);
        }
    }

    // ------------------------------------------------------------
    // Build sphere indices
    // ------------------------------------------------------------
    for (int i = 0; i < stacks; ++i) {
        int k1 = i * (sectors + 1);
        int k2 = k1 + sectors + 1;

        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                sphere_indices.push_back(k1);
                sphere_indices.push_back(k2);
                sphere_indices.push_back(k1 + 1);
            }

            if (i != (stacks - 1)) {
                sphere_indices.push_back(k1 + 1);
                sphere_indices.push_back(k2);
                sphere_indices.push_back(k2 + 1);
            }
        }
    }

    // ------------------------------------------------------------
    // Build final interleaved float array:
    // position(3), normal(3), color(3)
    // ------------------------------------------------------------
    std::vector<float> sphere_vertices;
    sphere_vertices.reserve(temp_vertices.size() * 9);

    for (const SphereBuildVertex& v : temp_vertices) {
        // position
        sphere_vertices.push_back(v.pos.x);
        sphere_vertices.push_back(v.pos.y);
        sphere_vertices.push_back(v.pos.z);

        // normal
        sphere_vertices.push_back(v.normal.x);
        sphere_vertices.push_back(v.normal.y);
        sphere_vertices.push_back(v.normal.z);

        // color
        sphere_vertices.push_back(v.color.x);
        sphere_vertices.push_back(v.color.y);
        sphere_vertices.push_back(v.color.z);
    }

    VertexLayout* sphere_vertex_layout = new VertexLayout();
    sphere_vertex_layout->add("position", 0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), 0,                 0, {0.0f, 0.0f, 0.0f});
    sphere_vertex_layout->add("normal",   1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), 3 * sizeof(float), 0, {0.0f, 1.0f, 0.0f});
    sphere_vertex_layout->add("color",    2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), 6 * sizeof(float), 0, {1.0f, 1.0f, 1.0f});

    mesh = new Mesh(sphere_vertices, sphere_indices, sphere_vertex_layout);
}