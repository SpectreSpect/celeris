#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace StaticMeshData{
    struct MeshData {
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
    };

    inline MeshData generate_uv_sphere(
        uint32_t sector_count = 64,
        uint32_t stack_count = 32,
        float radius = 0.5f)
    {
        sector_count = std::max(sector_count, 3u);
        stack_count = std::max(stack_count, 2u);

        constexpr float pi = 3.14159265358979323846f;

        MeshData mesh{};

        // Vertex layout:
        // glm::vec4 position;
        // glm::vec4 normal;
        // glm::vec2 uv;
        // glm::vec4 tangent;
        //
        // Total: 14 floats per vertex.
        mesh.vertices.reserve(
            static_cast<size_t>((stack_count + 1) * (sector_count + 1) * 14)
        );

        for (uint32_t stack = 0; stack <= stack_count; ++stack) {
            float v = static_cast<float>(stack) / static_cast<float>(stack_count);

            // phi goes from +pi/2 at the top to -pi/2 at the bottom.
            float phi = pi * 0.5f - v * pi;

            float cos_phi = std::cos(phi);
            float sin_phi = std::sin(phi);

            for (uint32_t sector = 0; sector <= sector_count; ++sector) {
                float u = static_cast<float>(sector) / static_cast<float>(sector_count);

                float theta = u * 2.0f * pi;

                float cos_theta = std::cos(theta);
                float sin_theta = std::sin(theta);

                float nx = cos_phi * cos_theta;
                float ny = sin_phi;
                float nz = cos_phi * sin_theta;

                float px = radius * nx;
                float py = radius * ny;
                float pz = radius * nz;

                // Tangent points in the increasing-U direction.
                float tx = -sin_theta;
                float ty = 0.0f;
                float tz = cos_theta;
                float tw = 1.0f;

                // position
                mesh.vertices.push_back(px);
                mesh.vertices.push_back(py);
                mesh.vertices.push_back(pz);
                mesh.vertices.push_back(1.0f);

                // normal
                mesh.vertices.push_back(nx);
                mesh.vertices.push_back(ny);
                mesh.vertices.push_back(nz);
                mesh.vertices.push_back(0.0f);

                // uv
                mesh.vertices.push_back(u);
                mesh.vertices.push_back(v);

                // tangent
                mesh.vertices.push_back(tx);
                mesh.vertices.push_back(ty);
                mesh.vertices.push_back(tz);
                mesh.vertices.push_back(tw);
            }
        }

        mesh.indices.reserve(static_cast<size_t>(sector_count * stack_count * 6));

        for (uint32_t stack = 0; stack < stack_count; ++stack) {
            uint32_t k1 = stack * (sector_count + 1);
            uint32_t k2 = k1 + sector_count + 1;

            for (uint32_t sector = 0; sector < sector_count; ++sector) {
                uint32_t i0 = k1 + sector;
                uint32_t i1 = k1 + sector + 1;
                uint32_t i2 = k2 + sector;
                uint32_t i3 = k2 + sector + 1;

                if (stack != 0) {
                    mesh.indices.push_back(i0);
                    mesh.indices.push_back(i2);
                    mesh.indices.push_back(i1);
                }

                if (stack != stack_count - 1) {
                    mesh.indices.push_back(i1);
                    mesh.indices.push_back(i2);
                    mesh.indices.push_back(i3);
                }
            }
        }

        return mesh;
    }

    inline MeshData sphere_mesh_data = generate_uv_sphere(64, 32, 0.5f);

    inline std::vector<float> sphere_vertices = sphere_mesh_data.vertices;
    inline std::vector<uint32_t> sphere_indices = sphere_mesh_data.indices;

    std::vector<float> cube_vertices = {
        // position                         normal                         uv          tangent

        // Front face, normal +Z, tangent +X
        -0.5f, -0.5f,  0.5f, 1.0f,         0.0f,  0.0f,  1.0f, 0.0f,      0.0f, 1.0f,  1.0f, 0.0f,  0.0f, -1.0f, // 0
        0.5f, -0.5f,  0.5f, 1.0f,         0.0f,  0.0f,  1.0f, 0.0f,      1.0f, 1.0f,  1.0f, 0.0f,  0.0f, -1.0f, // 1
        0.5f,  0.5f,  0.5f, 1.0f,         0.0f,  0.0f,  1.0f, 0.0f,      1.0f, 0.0f,  1.0f, 0.0f,  0.0f, -1.0f, // 2
        -0.5f,  0.5f,  0.5f, 1.0f,         0.0f,  0.0f,  1.0f, 0.0f,      0.0f, 0.0f,  1.0f, 0.0f,  0.0f, -1.0f, // 3

        // Back face, normal -Z, tangent -X
        0.5f, -0.5f, -0.5f, 1.0f,         0.0f,  0.0f, -1.0f, 0.0f,      0.0f, 1.0f, -1.0f, 0.0f,  0.0f, -1.0f, // 4
        -0.5f, -0.5f, -0.5f, 1.0f,         0.0f,  0.0f, -1.0f, 0.0f,      1.0f, 1.0f, -1.0f, 0.0f,  0.0f, -1.0f, // 5
        -0.5f,  0.5f, -0.5f, 1.0f,         0.0f,  0.0f, -1.0f, 0.0f,      1.0f, 0.0f, -1.0f, 0.0f,  0.0f, -1.0f, // 6
        0.5f,  0.5f, -0.5f, 1.0f,         0.0f,  0.0f, -1.0f, 0.0f,      0.0f, 0.0f, -1.0f, 0.0f,  0.0f, -1.0f, // 7

        // Left face, normal -X, tangent +Z
        -0.5f, -0.5f, -0.5f, 1.0f,        -1.0f,  0.0f,  0.0f, 0.0f,      0.0f, 1.0f,  0.0f, 0.0f,  1.0f, -1.0f, // 8
        -0.5f, -0.5f,  0.5f, 1.0f,        -1.0f,  0.0f,  0.0f, 0.0f,      1.0f, 1.0f,  0.0f, 0.0f,  1.0f, -1.0f, // 9
        -0.5f,  0.5f,  0.5f, 1.0f,        -1.0f,  0.0f,  0.0f, 0.0f,      1.0f, 0.0f,  0.0f, 0.0f,  1.0f, -1.0f, // 10
        -0.5f,  0.5f, -0.5f, 1.0f,        -1.0f,  0.0f,  0.0f, 0.0f,      0.0f, 0.0f,  0.0f, 0.0f,  1.0f, -1.0f, // 11

        // Right face, normal +X, tangent -Z
        0.5f, -0.5f,  0.5f, 1.0f,         1.0f,  0.0f,  0.0f, 0.0f,      0.0f, 1.0f,  0.0f, 0.0f, -1.0f, -1.0f, // 12
        0.5f, -0.5f, -0.5f, 1.0f,         1.0f,  0.0f,  0.0f, 0.0f,      1.0f, 1.0f,  0.0f, 0.0f, -1.0f, -1.0f, // 13
        0.5f,  0.5f, -0.5f, 1.0f,         1.0f,  0.0f,  0.0f, 0.0f,      1.0f, 0.0f,  0.0f, 0.0f, -1.0f, -1.0f, // 14
        0.5f,  0.5f,  0.5f, 1.0f,         1.0f,  0.0f,  0.0f, 0.0f,      0.0f, 0.0f,  0.0f, 0.0f, -1.0f, -1.0f, // 15

        // Top face, normal +Y, tangent +X
        -0.5f,  0.5f, -0.5f, 1.0f,         0.0f,  1.0f,  0.0f, 0.0f,      0.0f, 1.0f,  1.0f, 0.0f,  0.0f,  1.0f, // 16
        -0.5f,  0.5f,  0.5f, 1.0f,         0.0f,  1.0f,  0.0f, 0.0f,      0.0f, 0.0f,  1.0f, 0.0f,  0.0f,  1.0f, // 17
        0.5f,  0.5f,  0.5f, 1.0f,         0.0f,  1.0f,  0.0f, 0.0f,      1.0f, 0.0f,  1.0f, 0.0f,  0.0f,  1.0f, // 18
        0.5f,  0.5f, -0.5f, 1.0f,         0.0f,  1.0f,  0.0f, 0.0f,      1.0f, 1.0f,  1.0f, 0.0f,  0.0f,  1.0f, // 19

        // Bottom face, normal -Y, tangent +X
        -0.5f, -0.5f, -0.5f, 1.0f,         0.0f, -1.0f,  0.0f, 0.0f,      0.0f, 0.0f,  1.0f, 0.0f,  0.0f,  1.0f, // 20
        0.5f, -0.5f, -0.5f, 1.0f,         0.0f, -1.0f,  0.0f, 0.0f,      1.0f, 0.0f,  1.0f, 0.0f,  0.0f,  1.0f, // 21
        0.5f, -0.5f,  0.5f, 1.0f,         0.0f, -1.0f,  0.0f, 0.0f,      1.0f, 1.0f,  1.0f, 0.0f,  0.0f,  1.0f, // 22
        -0.5f, -0.5f,  0.5f, 1.0f,         0.0f, -1.0f,  0.0f, 0.0f,      0.0f, 1.0f,  1.0f, 0.0f,  0.0f,  1.0f  // 23
    };

    std::vector<uint32_t> cube_indices = {
        // Front face
        0, 1, 2,
        2, 3, 0,

        // Back face
        4, 5, 6,
        6, 7, 4,

        // Left face
        8, 9, 10,
        10, 11, 8,

        // Right face
        12, 13, 14,
        14, 15, 12,

        // Top face
        16, 17, 18,
        18, 19, 16,

        // Bottom face
        20, 21, 22,
        22, 23, 20
    };

    inline void append_vertex(
        std::vector<float>& v,
        float x, float y, float z)
    {
        // Позиция
        v.push_back(x);
        v.push_back(y);
        v.push_back(z);
        v.push_back(1.0f);

        // Нормаль
        v.push_back(0.0f);
        v.push_back(0.0f);
        v.push_back(1.0f);
        v.push_back(0.0f);

        // Текстурные координаты
        v.push_back(0.0f);
        v.push_back(0.0f);

        // Касательный вектор
        v.push_back(1.0f);
        v.push_back(0.0f);
        v.push_back(0.0f);
        v.push_back(1.0f);
    }

    inline void append_box(
        MeshData& mesh,
        float x0,
        float y0,
        float x1,
        float y1,
        float z0 = -0.1f,
        float z1 =  0.1f)
    {
        const uint32_t base = static_cast<uint32_t>(mesh.vertices.size() / 14);

        append_vertex(mesh.vertices, x0, y0, z1); // 0
        append_vertex(mesh.vertices, x1, y0, z1); // 1
        append_vertex(mesh.vertices, x1, y1, z1); // 2
        append_vertex(mesh.vertices, x0, y1, z1); // 3
        append_vertex(mesh.vertices, x0, y0, z0); // 4
        append_vertex(mesh.vertices, x1, y0, z0); // 5
        append_vertex(mesh.vertices, x1, y1, z0); // 6
        append_vertex(mesh.vertices, x0, y1, z0); // 7

        uint32_t indices[] = {
            // Перед
            0,1,2, 2,3,0,
            // Зад
            5,4,7, 7,6,5,
            // Лево
            4,0,3, 3,7,4,
            // Право
            1,5,6, 6,2,1,
            // Верх
            3,2,6, 6,7,3,
            // Низ
            4,5,1, 1,0,4
        };

        for(uint32_t index : indices)
            mesh.indices.push_back(base + index);
    }

    inline MeshData make_C_letter() {
        MeshData mesh{};

        append_box(mesh, -0.8f,  0.6f,  0.8f,  0.8f);
        append_box(mesh, -0.8f, -0.8f, -0.6f,  0.8f);
        append_box(mesh, -0.8f, -0.8f,  0.8f, -0.6f);

        return mesh;
    }
    inline MeshData make_E_letter() {
        MeshData mesh{};

        append_box(mesh, -0.8f,  0.6f,  0.8f,  0.8f);
        append_box(mesh, -0.8f, -0.8f, -0.6f,  0.8f);
        append_box(mesh, -0.8f, -0.1f,  0.4f,  0.1f);
        append_box(mesh, -0.8f, -0.8f,  0.8f, -0.6f);

        return mesh;
    }
    inline MeshData make_L_letter() {
        MeshData mesh{};

        append_box(mesh, -0.8f, -0.8f, -0.6f,  0.8f);
        append_box(mesh, -0.8f, -0.8f,  0.8f, -0.6f);

        return mesh;
    }
    inline MeshData make_I_letter() {
        MeshData mesh{};

        append_box(mesh, -0.8f,  0.6f, 0.8f,  0.8f);
        append_box(mesh, -0.1f, -0.8f, 0.1f,  0.8f);
        append_box(mesh, -0.8f, -0.8f, 0.8f, -0.6f);

        return mesh;
    }
    inline MeshData make_R_letter() {
        MeshData mesh{};

        append_box(mesh, -0.8f, -0.8f, -0.6f,  0.8f);
        append_box(mesh, -0.8f,  0.6f,  0.6f,  0.8f);
        append_box(mesh,  0.4f,  0.1f,  0.6f,  0.8f);
        append_box(mesh, -0.8f, -0.1f,  0.6f,  0.1f);
        append_box(mesh,  0.0f, -0.8f,  0.2f,  0.1f);
        append_box(mesh,  0.2f, -0.8f,  0.6f, -0.6f);

        return mesh;
    }
    inline MeshData make_S_letter() {
        MeshData mesh{};

        append_box(mesh, -0.8f,  0.6f,  0.8f,  0.8f);
        append_box(mesh, -0.8f,  0.1f, -0.6f,  0.8f);
        append_box(mesh, -0.8f, -0.1f,  0.8f,  0.1f);
        append_box(mesh,  0.6f, -0.8f,  0.8f, -0.1f);
        append_box(mesh, -0.8f, -0.8f,  0.8f, -0.6f);

        return mesh;
    }

    inline void append_mesh(
        MeshData &dst,
        const MeshData &src,
        float offset_x)
    {
        uint32_t base = static_cast<uint32_t>(dst.vertices.size() / 14);

        for(uint32_t i = 0; i < src.vertices.size(); i += 14) {
            dst.vertices.push_back(src.vertices[i + 0] + offset_x);
            dst.vertices.push_back(src.vertices[i + 1]);
            dst.vertices.push_back(src.vertices[i + 2]);
            dst.vertices.push_back(src.vertices[i + 3]);

            for(int j = 4; j < 14; ++j)
                dst.vertices.push_back(src.vertices[i + j]);
        }

        for(uint32_t index : src.indices)
            dst.indices.push_back(base + index);
    }

    inline MeshData generate_celeris_text() {
        MeshData mesh{};

        append_mesh(mesh, make_C_letter(), -7.0f);
        append_mesh(mesh, make_E_letter(), -4.5f);
        append_mesh(mesh, make_L_letter(), -2.0f);
        append_mesh(mesh, make_E_letter(),  0.5f);
        append_mesh(mesh, make_R_letter(),  3.0f);
        append_mesh(mesh, make_I_letter(),  5.5f);
        append_mesh(mesh, make_S_letter(),  8.0f);

        return mesh;
    }

    inline MeshData celeris_mesh_data = generate_celeris_text();

    inline std::vector<float> text_vertices = celeris_mesh_data.vertices;
    inline std::vector<uint32_t> text_indices = celeris_mesh_data.indices;

    std::vector<float> point_cloud_quad_corners = { // vertex buffer
        -1.0f, -1.0f,  // v0
        -1.0f, +1.0f,  // v1
        +1.0f, -1.0f,  // v2
        +1.0f, +1.0f   // v3
    };

    std::vector<uint32_t> point_cloud_quad_indices = { // index_buffer
        0, 1, 2,
        2, 1, 3
    };

    
    std::vector<float> skybox_cube_vertices = {
        // position.x, position.y, position.z, position.w

        -1.0f, -1.0f, -1.0f, 1.0f, // 0
        1.0f, -1.0f, -1.0f, 1.0f, // 1
        1.0f,  1.0f, -1.0f, 1.0f, // 2
        -1.0f,  1.0f, -1.0f, 1.0f, // 3

        -1.0f, -1.0f,  1.0f, 1.0f, // 4
        1.0f, -1.0f,  1.0f, 1.0f, // 5
        1.0f,  1.0f,  1.0f, 1.0f, // 6
        -1.0f,  1.0f,  1.0f, 1.0f  // 7
    };

    std::vector<uint32_t> skybox_cube_indices = {
        // Back face, -Z
        0, 2, 1,
        2, 0, 3,

        // Front face, +Z
        4, 5, 6,
        6, 7, 4,

        // Left face, -X
        0, 4, 7,
        7, 3, 0,

        // Right face, +X
        1, 2, 6,
        6, 5, 1,

        // Top face, +Y
        3, 7, 6,
        6, 2, 3,

        // Bottom face, -Y
        0, 1, 5,
        5, 4, 0
    };
}