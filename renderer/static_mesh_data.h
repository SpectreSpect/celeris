#pragma once
#include <vector>
#include <cstdint>
#include <cstddef>
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


    struct TwoSphereIndirectTestData {
        MeshData mesh;

        uint32_t sphere_vertex_count = 0;
        uint32_t sphere_index_count = 0;

        uint32_t first_sphere_first_index = 0;
        int32_t  first_sphere_base_vertex = 0;

        uint32_t second_sphere_first_index = 0;
        int32_t  second_sphere_base_vertex = 0;
    };

    inline void translate_mesh_positions_x(MeshData& mesh, float x_offset) {
        constexpr uint32_t vertex_stride_floats = 14;
        constexpr uint32_t position_x_offset = 0;

        for (size_t i = 0; i < mesh.vertices.size(); i += vertex_stride_floats) {
            mesh.vertices[i + position_x_offset] += x_offset;
        }
    }

    inline TwoSphereIndirectTestData generate_two_sphere_indirect_test(
        uint32_t sector_count = 64,
        uint32_t stack_count = 32,
        float radius = 0.5f,
        float spacing = 1.5f)
    {
        MeshData left_sphere = generate_uv_sphere(sector_count, stack_count, radius);
        MeshData right_sphere = generate_uv_sphere(sector_count, stack_count, radius);

        translate_mesh_positions_x(left_sphere, -spacing * 0.5f);
        translate_mesh_positions_x(right_sphere, spacing * 0.5f);

        TwoSphereIndirectTestData result{};
        result.sphere_vertex_count = static_cast<uint32_t>(left_sphere.vertices.size() / 14);
        result.sphere_index_count = static_cast<uint32_t>(left_sphere.indices.size());

        result.first_sphere_first_index = 0;
        result.first_sphere_base_vertex = 0;

        result.second_sphere_first_index = result.sphere_index_count;
        result.second_sphere_base_vertex = static_cast<int32_t>(result.sphere_vertex_count);

        result.mesh.vertices.reserve(left_sphere.vertices.size() + right_sphere.vertices.size());
        result.mesh.vertices.insert(result.mesh.vertices.end(), left_sphere.vertices.begin(), left_sphere.vertices.end());
        result.mesh.vertices.insert(result.mesh.vertices.end(), right_sphere.vertices.begin(), right_sphere.vertices.end());

        // Important for the indirect test:
        // The second sphere deliberately reuses local 0-based indices.
        // Its indirect command must use baseVertex = second_sphere_base_vertex.
        result.mesh.indices.reserve(left_sphere.indices.size() + right_sphere.indices.size());
        result.mesh.indices.insert(result.mesh.indices.end(), left_sphere.indices.begin(), left_sphere.indices.end());
        result.mesh.indices.insert(result.mesh.indices.end(), right_sphere.indices.begin(), right_sphere.indices.end());

        return result;
    }

    // User-facing test mesh. Kept with the original requested typo in the name.
    inline TwoSphereIndirectTestData two_sphere_inidirect_test = generate_two_sphere_indirect_test();
    inline MeshData two_sphere_inidirect_test_mesh_data = two_sphere_inidirect_test.mesh;
    inline std::vector<float> two_sphere_inidirect_test_vertices = two_sphere_inidirect_test_mesh_data.vertices;
    inline std::vector<uint32_t> two_sphere_inidirect_test_indices = two_sphere_inidirect_test_mesh_data.indices;

    // Correctly spelled aliases, in case you want to use the fixed name later.
    inline TwoSphereIndirectTestData& two_sphere_indirect_test = two_sphere_inidirect_test;
    inline MeshData& two_sphere_indirect_test_mesh_data = two_sphere_inidirect_test_mesh_data;
    inline std::vector<float>& two_sphere_indirect_test_vertices = two_sphere_inidirect_test_vertices;
    inline std::vector<uint32_t>& two_sphere_indirect_test_indices = two_sphere_inidirect_test_indices;

    inline std::vector<float> cube_vertices = {
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

    inline std::vector<uint32_t> cube_indices = {
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

    inline std::vector<float> point_cloud_quad_corners = { // vertex buffer
        -1.0f, -1.0f,  // v0
        -1.0f, +1.0f,  // v1
        +1.0f, -1.0f,  // v2
        +1.0f, +1.0f   // v3
    };

    inline std::vector<uint32_t> point_cloud_quad_indices = { // index_buffer
        0, 1, 2,
        2, 1, 3
    };

    
    inline std::vector<float> skybox_cube_vertices = {
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

    inline std::vector<uint32_t> skybox_cube_indices = {
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

    inline std::vector<float> line_quad_vertices = {
        0.0f, -1.0f,
        1.0f, -1.0f,
        1.0f,  1.0f,
        0.0f,  1.0f
    };

    inline std::vector<uint32_t> line_quad_indices = {
        0, 1, 2,
        2, 3, 0
    };
}