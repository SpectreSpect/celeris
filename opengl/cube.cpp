#include "cube.h"

<<<<<<< HEAD:cube.cpp
Cube::Cube(glm::vec3 position, glm::vec3 scale, glm::vec3 color) 
    :   Transformable(position, scale, glm::vec3(0)),
        _color(color),
        mesh(Cube::create_mesh(color))
{
    
=======
Cube::Cube(glm::vec3 position, glm::vec3 scale, glm::vec3 color) {
    this->position = position;
    this->scale = scale;
    this->_color = color;

    // MeshData mesh_data = create_mesh_data(color);

    // VertexLayout* vertex_layout = new VertexLayout();
    // vertex_layout->add(
    //     "position",
    //     0, 3, GL_FLOAT, GL_FALSE,
    //     9 * sizeof(float),
    //     0, 
    //     0, {0.0f, 0.0f, 0.0f}
    // );
    // vertex_layout->add(
    //     "normal",
    //     1, 3, GL_FLOAT, GL_FALSE,
    //     9 * sizeof(float),
    //     3 * sizeof(float),
    //     0, {0.0f, 1.0f, 0.0f}
    // );
    // vertex_layout->add(
    //     "color",
    //     2, 3, GL_FLOAT, GL_FALSE,
    //     9 * sizeof(float),
    //     6 * sizeof(float),
    //     0, {1.0f, 1.0f, 1.0f}
    // );

    // mesh = new Mesh(mesh_data.vertices, mesh_data.indices, vertex_layout);



    static std::vector<float> vertices = {
        // 0
        -1.0f, -1.0f, -1.0f,
        // 1
         1.0f, -1.0f, -1.0f,
        // 2
         1.0f,  1.0f, -1.0f,
        // 3
        -1.0f,  1.0f, -1.0f,
        // 4
        -1.0f, -1.0f,  1.0f,
        // 5
         1.0f, -1.0f,  1.0f,
        // 6
         1.0f,  1.0f,  1.0f,
        // 7
        -1.0f,  1.0f,  1.0f
    };

    static std::vector<unsigned int> indices = {
        // -Z
        0, 2, 1,
        0, 3, 2,

        // +Z
        4, 5, 6,
        4, 6, 7,

        // -X
        0, 7, 3,
        0, 4, 7,

        // +X
        1, 2, 6,
        1, 6, 5,

        // +Y
        3, 7, 6,
        3, 6, 2,

        // -Y
        0, 1, 5,
        0, 5, 4
    };

    VertexLayout* vertex_layout = new VertexLayout();
    vertex_layout->add("position", 0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0, 0, {0.0f, 0.0f, 0.0f});

    mesh = new Mesh(vertices, indices, vertex_layout);
>>>>>>> origin/cloud-to-mesh:opengl/cube.cpp
}

MeshData Cube::create_mesh_data(glm::vec3 color) {
    std::vector<float> vertices = {
        // positions            // normals

        // Front (+Z)
        -1, -1,  1,   0,  0,  1, color.x, color.y, color.z,
        1, -1,  1,   0,  0,  1, color.x, color.y, color.z,
        1,  1,  1,   0,  0,  1, color.x, color.y, color.z,
        -1,  1,  1,   0,  0,  1, color.x, color.y, color.z,

        // Back (-Z)
        1, -1, -1,   0,  0, -1, color.x, color.y, color.z,
        -1, -1, -1,   0,  0, -1, color.x, color.y, color.z,
        -1,  1, -1,   0,  0, -1, color.x, color.y, color.z,
        1,  1, -1,   0,  0, -1, color.x, color.y, color.z,

        // Left (-X)
        -1, -1, -1,  -1,  0,  0, color.x, color.y, color.z,
        -1, -1,  1,  -1,  0,  0, color.x, color.y, color.z,
        -1,  1,  1,  -1,  0,  0, color.x, color.y, color.z,
        -1,  1, -1,  -1,  0,  0, color.x, color.y, color.z,

        // Right (+X)
        1, -1,  1,   1,  0,  0, color.x, color.y, color.z,
        1, -1, -1,   1,  0,  0, color.x, color.y, color.z,
        1,  1, -1,   1,  0,  0, color.x, color.y, color.z,
        1,  1,  1,   1,  0,  0, color.x, color.y, color.z,
        // Top (+Y)
        -1,  1,  1,   0,  1,  0, color.x, color.y, color.z,
        1,  1,  1,   0,  1,  0, color.x, color.y, color.z,
        1,  1, -1,   0,  1,  0, color.x, color.y, color.z,
        -1,  1, -1,   0,  1,  0, color.x, color.y, color.z,

        // Bottom (-Y)
        -1, -1, -1,   0, -1,  0, color.x, color.y, color.z,
        1, -1, -1,   0, -1,  0, color.x, color.y, color.z,
        1, -1,  1,   0, -1,  0, color.x, color.y, color.z,
        -1, -1,  1,   0, -1,  0, color.x, color.y, color.z,
    };

    std::vector<unsigned int> indices = {
        0,  1,  2,  2,  3,  0,   // front
        4,  5,  6,  6,  7,  4,   // back
        8,  9, 10, 10, 11,  8,   // left
        12, 13, 14, 14, 15, 12,   // right
        16, 17, 18, 18, 19, 16,   // top
        20, 21, 22, 22, 23, 20    // bottom
    };
    
    MeshData mesh_data;
    mesh_data.vertices = std::move(vertices);
    mesh_data.indices = std::move(indices);

    return mesh_data;
}

Mesh Cube::create_mesh(glm::vec3 color) {
    MeshData mesh_data = Cube::create_mesh_data(color);

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

    return Mesh(mesh_data.vertices, mesh_data.indices, vertex_layout);
}

void Cube::set_color(glm::vec3& color) {
    _color = color;
    MeshData mesh_data = create_mesh_data(color);
    mesh.update(mesh_data.vertices, mesh_data.indices);
}

void Cube::draw(RenderState state) {
    state.transform *= get_model_matrix();
    mesh.draw(state);
}