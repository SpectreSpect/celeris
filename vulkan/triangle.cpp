#include "triangle.h"

Triangle::Triangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 color1, glm::vec3 color2, glm::vec3 color3) {
    this->mesh = create_triangle_mesh(v0, v1, v2, color1, color2, color3);
    this->v0_ = v0;
    this->v1_ = v1;
    this->v2_ = v2;
    this->color1_ = color1;
    this->color2_ = color2;
    this->color3_ = color3;
}

Triangle::~Triangle() {
    delete mesh;
}

Mesh* Triangle::create_triangle_mesh(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 color1, glm::vec3 color2, glm::vec3 color3) {
    glm::vec3 e1 = v1 - v0;
    glm::vec3 e2 = v2 - v0;
    glm::vec3 normal  = glm::normalize(glm::cross(e1, e2));

    glm::vec3 points[3] = {v0, v1, v2}; 
    glm::vec3 colors[3] = {color1, color2, color3}; 
    
    std::vector<float> vertices;
    vertices.reserve(3 * 9);

    std::vector<unsigned int> indices = {0, 1, 2};

    for (int i = 0; i < 3; i++) {
        vertices.push_back(points[i].x);
        vertices.push_back(points[i].y);
        vertices.push_back(points[i].z);

        vertices.push_back(normal.x);
        vertices.push_back(normal.y);
        vertices.push_back(normal.z);

        vertices.push_back(colors[i].x);
        vertices.push_back(colors[i].y);
        vertices.push_back(colors[i].z);
    }

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

    Mesh* triangle = new Mesh(vertices, indices, vertex_layout);

    return triangle;
}

void Triangle::update_vbo(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 color1, glm::vec3 color2, glm::vec3 color3) {
    delete this->mesh;
    this->mesh = create_triangle_mesh(v0, v1, v2, color1, color2, color3);
    this->v0_ = v0;
    this->v1_ = v1;
    this->v2_ = v2;
    this->color1_ = color1;
    this->color2_ = color2;
    this->color3_ = color3;
}

void Triangle::draw(RenderState state) {
    state.transform *= get_model_matrix();
    mesh->draw(state);
}