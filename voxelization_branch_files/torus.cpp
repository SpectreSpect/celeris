#include "torus.h"

Torus::Torus(
    const glm::vec3& position, 
    const glm::vec3& scale, 
    const glm::vec3& rotation,
    float radius,
    float tube_radius,
    uint32_t detail,
    glm::vec3 color
) 
    :   Transformable(position, scale, rotation),
        mesh(create_torus_mesh(radius, tube_radius, detail, color))
{

}

Mesh Torus::create_torus_mesh(float radius, float tube_radius, uint32_t detail, glm::vec3 color) {
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    VertexLayout vertex_layout;

    vertex_layout.add(
        "position",
        0, 3, GL_FLOAT, GL_FALSE,
        9 * sizeof(float),
        0,
        0, {0.0f, 0.0f, 0.0f}
    );
    vertex_layout.add(
        "normal",
        1, 3, GL_FLOAT, GL_FALSE,
        9 * sizeof(float),
        3 * sizeof(float),
        0, {0.0f, 1.0f, 0.0f}
    );
    vertex_layout.add(
        "color",
        2, 3, GL_FLOAT, GL_FALSE,
        9 * sizeof(float),
        6 * sizeof(float),
        0, {1.0f, 1.0f, 1.0f}
    );

    const uint32_t major_segments = std::max(3u, detail);
    const uint32_t minor_segments = std::max(3u, detail);

    const float pi = 3.14159265358979323846f;
    const float two_pi = 2.0f * pi;

    vertices.reserve(static_cast<size_t>(major_segments) * minor_segments * 9);
    indices.reserve(static_cast<size_t>(major_segments) * minor_segments * 6);

    for (uint32_t i = 0; i < major_segments; ++i) {
        float u = (static_cast<float>(i) / major_segments) * two_pi;
        float cu = std::cos(u);
        float su = std::sin(u);

        for (uint32_t j = 0; j < minor_segments; ++j) {
            float v = (static_cast<float>(j) / minor_segments) * two_pi;
            float cv = std::cos(v);
            float sv = std::sin(v);

            // Позиция вершины
            float x = (radius + tube_radius * cv) * cu;
            float y = tube_radius * sv;
            float z = (radius + tube_radius * cv) * su;

            // Нормаль
            float nx = cv * cu;
            float ny = sv;
            float nz = cv * su;

            // position
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            // normal
            vertices.push_back(nx);
            vertices.push_back(ny);
            vertices.push_back(nz);

            // color
            vertices.push_back(color.x);
            vertices.push_back(color.y);
            vertices.push_back(color.z);
        }
    }

    for (uint32_t i = 0; i < major_segments; ++i) {
        uint32_t next_i = (i + 1) % major_segments;

        for (uint32_t j = 0; j < minor_segments; ++j) {
            uint32_t next_j = (j + 1) % minor_segments;

            uint32_t a = i * minor_segments + j;
            uint32_t b = next_i * minor_segments + j;
            uint32_t c = next_i * minor_segments + next_j;
            uint32_t d = i * minor_segments + next_j;

            // Первый треугольник
            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(c);

            // Второй треугольник
            indices.push_back(a);
            indices.push_back(c);
            indices.push_back(d);
        }
    }

    return Mesh(vertices, indices, vertex_layout);
}

void Torus::draw(RenderState state)  {
    state.transform *= get_model_matrix();

    mesh.draw(state);
}