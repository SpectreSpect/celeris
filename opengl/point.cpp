#include "point.h"
#include "engine3d.h"

Point::Point() {
    // Quad corners in "local sprite space" (for billboard)
    // corner.x, corner.y in [-1..+1]
    const float quadCorners[] = {
        -1.0f, -1.0f,  // v0
        -1.0f, +1.0f,  // v1
        +1.0f, -1.0f,  // v2
        +1.0f, +1.0f   // v3
    };

    const unsigned int quadIndices[] = {
        0, 1, 2,
        2, 1, 3
    };

    // Dummy instance so VBO(data,size) is valid
    PointInstance dummy{};
    // dummy.pos   = {0, 0, 0};
    // dummy.color = {1, 1, 1};
    dummy.pos   = {0, 0, 0, 0};
    dummy.color = {1, 1, 1, 1};

    vao = new VAO();
    quad_vbo = new VBO(quadCorners, sizeof(quadCorners));
    quad_ebo = new EBO(quadIndices, sizeof(quadIndices));
    instance_vbo = new VBO(&dummy, sizeof(PointInstance)); // dynamic updates later

    vao->init_vao();

    vao->bind();

    // EBO binding is stored in VAO
    quad_ebo->bind();

    // Per-vertex attribute: corner (location = 1)
    quad_vbo->bind();
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE,
        2 * sizeof(float),
        (void*)0
    );
    glVertexAttribDivisor(1, 0); // per-vertex

    // Per-instance attributes: pos (loc=0), color (loc=2)
    instance_vbo->bind();

    // aPos (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        sizeof(PointInstance),
        (void*)offsetof(PointInstance, pos)
    );
    glVertexAttribDivisor(0, 1); // per-instance

    // aColor (location = 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2, 3, GL_FLOAT, GL_FALSE,
        sizeof(PointInstance),
        (void*)offsetof(PointInstance, color)
    );
    glVertexAttribDivisor(2, 1); // per-instance

    vao->unbind();

    instance_count = 0;
}



Point::Point(SSBO& instance_ssbo, int instance_count) {
    this->instance_count = instance_count;
    // Quad corners in "local sprite space" (for billboard)
    // corner.x, corner.y in [-1..+1]
    const float quadCorners[] = {
        -1.0f, -1.0f,  // v0
        -1.0f, +1.0f,  // v1
        +1.0f, -1.0f,  // v2
        +1.0f, +1.0f   // v3
    };

    const unsigned int quadIndices[] = {
        0, 1, 2,
        2, 1, 3
    };

    // Dummy instance so VBO(data,size) is valid
    PointInstance dummy{};
    // dummy.pos   = {0, 0, 0};
    // dummy.color = {1, 1, 1};
    dummy.pos   = {0, 0, 0, 0};
    dummy.color = {1, 1, 1, 1};

    vao = new VAO();
    quad_vbo = new VBO(quadCorners, sizeof(quadCorners));
    quad_ebo = new EBO(quadIndices, sizeof(quadIndices));
    instance_vbo = new VBO(instance_ssbo);
    // instance_vbo = new VBO(&dummy, sizeof(PointInstance)); // dynamic updates later

    vao->init_vao();

    vao->bind();

    // EBO binding is stored in VAO
    quad_ebo->bind();

    // Per-vertex attribute: corner (location = 1)
    quad_vbo->bind();
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE,
        2 * sizeof(float),
        (void*)0
    );
    glVertexAttribDivisor(1, 0); // per-vertex

    // Per-instance attributes: pos (loc=0), color (loc=2)
    instance_vbo->bind();

    // aPos (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        sizeof(PointInstance),
        (void*)offsetof(PointInstance, pos)
    );
    glVertexAttribDivisor(0, 1); // per-instance

    // aColor (location = 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2, 3, GL_FLOAT, GL_FALSE,
        sizeof(PointInstance),
        (void*)offsetof(PointInstance, color)
    );
    glVertexAttribDivisor(2, 1); // per-instance

    vao->unbind();

    instance_count = 0;
}

Point::~Point() {
    delete vao;
    delete quad_vbo;
    delete quad_ebo;
    delete instance_vbo;
}

void Point::set_points(const std::vector<PointInstance>& points) {
    instance_count = (int)points.size();
    if (instance_count == 0) return;

    instance_vbo->update_mapped(
        points.data(),
        points.size() * sizeof(PointInstance),
        GL_STREAM_DRAW
    );
}

void Point::set_points(SSBO& points, int num_points) {
    instance_count = num_points;
    if (instance_count == 0) return;
    
    instance_vbo->id = points.id_;
    instance_vbo->size_bytes = points.size_bytes_;
}

void Point::draw(RenderState state) {
    if (instance_count <= 0) return;

    Program* program = state.engine->default_point_program;
    program->use();

    // --- Apply Transformable like Mesh does ---
    glm::mat4 model = get_model_matrix();
    glm::mat4 world = state.transform * model;

    // Your point shader must use this (see snippet below)
    program->set_mat4("uModel", world);

    program->set_mat4("uProj", state.proj);
    if (state.camera) {
        program->set_mat4("uView", state.camera->get_view_matrix());
    }
    
    program->set_vec2("uViewport", state.viewport_px);

    program->set_int("uScreenSpaceSize", constant_screen_size ? 1 : 0);
    program->set_float("uPointSizePx", size_px);
    program->set_float("uPointSizeWorld", size_world);
    program->set_int("uRound", round ? 1 : 0);

    vao->bind();
    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, instance_count);
    vao->unbind();
}
