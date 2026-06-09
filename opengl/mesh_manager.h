#pragma once
#include "unordered_map"
#include "iostream"
// #include "mesh.h"
#include "memory"

class Mesh;

class MeshManager {
public:
    std::unordered_map<std::string, std::unique_ptr<Mesh>> meshes;

    Mesh* get(const std::string& name) {
        return meshes.at(name).get();
    }

    Mesh* load_cube() {
        auto it = meshes.find("cube");
        if (it != meshes.end())
            return it->second.get();
        
        std::vector<float> vertices_vec = {
        // positions            // normals

            // Front (+Z)
            -1, -1,  1,   0,  0,  1,
            1, -1,  1,   0,  0,  1,
            1,  1,  1,   0,  0,  1,
            -1,  1,  1,   0,  0,  1,

            // Back (-Z)
            1, -1, -1,   0,  0, -1,
            -1, -1, -1,   0,  0, -1,
            -1,  1, -1,   0,  0, -1,
            1,  1, -1,   0,  0, -1,

            // Left (-X)
            -1, -1, -1,  -1,  0,  0,
            -1, -1,  1,  -1,  0,  0,
            -1,  1,  1,  -1,  0,  0,
            -1,  1, -1,  -1,  0,  0,

            // Right (+X)
            1, -1,  1,   1,  0,  0,
            1, -1, -1,   1,  0,  0,
            1,  1, -1,   1,  0,  0,
            1,  1,  1,   1,  0,  0,

            // Top (+Y)
            -1,  1,  1,   0,  1,  0,
            1,  1,  1,   0,  1,  0,
            1,  1, -1,   0,  1,  0,
            -1,  1, -1,   0,  1,  0,

            // Bottom (-Y)
            -1, -1, -1,   0, -1,  0,
            1, -1, -1,   0, -1,  0,
            1, -1,  1,   0, -1,  0,
            -1, -1,  1,   0, -1,  0,
        };

        std::vector<unsigned int> indices_vec = {
            0,  1,  2,  2,  3,  0,   // front
            4,  5,  6,  6,  7,  4,   // back
            8,  9, 10, 10, 11,  8,   // left
            12, 13, 14, 14, 15, 12,   // right
            16, 17, 18, 18, 19, 16,   // top
            20, 21, 22, 22, 23, 20    // bottom
        };

        VertexLayout* vertex_layout = new VertexLayout();
        vertex_layout->add(
            "position",
            0, 3, GL_FLOAT, GL_FALSE,
            6 * sizeof(float),
            0,
            0, {0.0f, 0.0f, 0.0f}
        );
        vertex_layout->add(
            "normal",
            1, 3, GL_FLOAT, GL_FALSE,
            6 * sizeof(float),
            3 * sizeof(float),
            0, {0.0f, 1.0f, 0.0f}
        );


        meshes["cube"] = std::make_unique<Mesh>(vertices_vec, indices_vec, vertex_layout);
        return meshes["cube"].get();
    }
};
