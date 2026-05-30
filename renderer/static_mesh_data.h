#pragma once
#include <vector>
#include <cstdint>

namespace StaticMeshData{
    std::vector<float> cube_vertices = {
        // Front face, normal +Z
        -0.5f, -0.5f,  0.5f, 1.0f,   0.0f, 0.0f, 1.0f, 0.0f,   0.0f, 1.0f, // 0
        0.5f, -0.5f,  0.5f, 1.0f,   0.0f, 0.0f, 1.0f, 0.0f,   1.0f, 1.0f, // 1
        0.5f,  0.5f,  0.5f, 1.0f,   0.0f, 0.0f, 1.0f, 0.0f,   1.0f, 0.0f, // 2
        -0.5f,  0.5f,  0.5f, 1.0f,   0.0f, 0.0f, 1.0f, 0.0f,   0.0f, 0.0f, // 3

        // Back face, normal -Z
        0.5f, -0.5f, -0.5f, 1.0f,   0.0f, 0.0f, -1.0f, 0.0f,   0.0f, 1.0f, // 4
        -0.5f, -0.5f, -0.5f, 1.0f,   0.0f, 0.0f, -1.0f, 0.0f,   1.0f, 1.0f, // 5
        -0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 0.0f, -1.0f, 0.0f,   1.0f, 0.0f, // 6
        0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 0.0f, -1.0f, 0.0f,   0.0f, 0.0f, // 7

        // Left face, normal -X
        -0.5f, -0.5f, -0.5f, 1.0f,  -1.0f, 0.0f, 0.0f, 0.0f,   0.0f, 1.0f, // 8
        -0.5f, -0.5f,  0.5f, 1.0f,  -1.0f, 0.0f, 0.0f, 0.0f,   1.0f, 1.0f, // 9
        -0.5f,  0.5f,  0.5f, 1.0f,  -1.0f, 0.0f, 0.0f, 0.0f,   1.0f, 0.0f, // 10
        -0.5f,  0.5f, -0.5f, 1.0f,  -1.0f, 0.0f, 0.0f, 0.0f,   0.0f, 0.0f, // 11

        // Right face, normal +X
        0.5f, -0.5f,  0.5f, 1.0f,   1.0f, 0.0f, 0.0f, 0.0f,   0.0f, 1.0f, // 12
        0.5f, -0.5f, -0.5f, 1.0f,   1.0f, 0.0f, 0.0f, 0.0f,   1.0f, 1.0f, // 13
        0.5f,  0.5f, -0.5f, 1.0f,   1.0f, 0.0f, 0.0f, 0.0f,   1.0f, 0.0f, // 14
        0.5f,  0.5f,  0.5f, 1.0f,   1.0f, 0.0f, 0.0f, 0.0f,   0.0f, 0.0f, // 15

        // Top face, normal +Y
        -0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 0.0f,   0.0f, 1.0f, // 16
        -0.5f,  0.5f,  0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 0.0f,   0.0f, 0.0f, // 17
        0.5f,  0.5f,  0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 0.0f,   1.0f, 0.0f, // 18
        0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 0.0f,   1.0f, 1.0f, // 19

        // Bottom face, normal -Y
        -0.5f, -0.5f, -0.5f, 1.0f,   0.0f, -1.0f, 0.0f, 0.0f,   0.0f, 0.0f, // 20
        0.5f, -0.5f, -0.5f, 1.0f,   0.0f, -1.0f, 0.0f, 0.0f,   1.0f, 0.0f, // 21
        0.5f, -0.5f,  0.5f, 1.0f,   0.0f, -1.0f, 0.0f, 0.0f,   1.0f, 1.0f, // 22
        -0.5f, -0.5f,  0.5f, 1.0f,   0.0f, -1.0f, 0.0f, 0.0f,   0.0f, 1.0f, // 23
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