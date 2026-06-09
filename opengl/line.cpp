#include "line.h"
#include "engine3d.h"

Line::Line() {
    // 4 vertices for a quad (triangle strip style corners),
    // but we'll use indexed triangles:
    // corner.x = endpoint selector (0 or 1)
    // corner.y = side sign (-1 or +1)
    const float quadCorners[] = {
        0.0f, -1.0f,  // v0
        0.0f, +1.0f,  // v1
        1.0f, -1.0f,  // v2
        1.0f, +1.0f   // v3
    };

    const unsigned int quadIndices[] = {
        0, 1, 2,
        2, 1, 3
    };

    // Dummy instance so VBO(data,size) is valid
    LineInstance dummy{};
    dummy.p0 = {0,0,0};
    dummy.p1 = {0,0,0};

    vao = new VAO();
    quad_vbo = new VBO(quadCorners, sizeof(quadCorners));
    quad_ebo = new EBO(quadIndices, sizeof(quadIndices));
    instance_vbo = new VBO(&dummy, sizeof(LineInstance)); // dynamic updates later

    vao->init_vao();

    // --- VAO wiring (manual, because we have 2 VBOs + instancing) ---
    vao->bind();

    // EBO binding is stored in VAO
    quad_ebo->bind();

    // Per-vertex attribute: corner (location = 2)
    quad_vbo->bind();
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2, 2, GL_FLOAT, GL_FALSE,
        2 * sizeof(float),
        (void*)0
    );
    glVertexAttribDivisor(2, 0); // per-vertex

    // Per-instance attributes: p0 (loc=0), p1 (loc=1)
    instance_vbo->bind();

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        sizeof(LineInstance),
        (void*)offsetof(LineInstance, p0)
    );
    glVertexAttribDivisor(0, 1); // per-instance

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE,
        sizeof(LineInstance),
        (void*)offsetof(LineInstance, p1)
    );
    glVertexAttribDivisor(1, 1); // per-instance

    vao->unbind();

    instance_count = 0;
}

Line::~Line() {
    delete vao;
    delete quad_vbo;
    delete quad_ebo;
    delete instance_vbo;
}

void Line::set_lines(const std::vector<LineInstance>& lines) {
    instance_count = (int)lines.size();
    if (instance_count == 0) return;

    instance_vbo->update_mapped(lines.data(),
                               lines.size() * sizeof(LineInstance),
                               GL_STREAM_DRAW);
}


// void Line::draw_instanced() const {
//     if (instance_count <= 0) return;

//     vao->bind();
//     glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, instance_count);
//     vao->unbind();
// }


void Line::draw(RenderState state) {
    if (instance_count <= 0) return;

    Program* program = state.engine->default_line_program;
    program->use();
    

    glm::mat4 model = get_model_matrix();
    glm::mat4 world = state.transform * model;

    program->set_mat4("uModel", world);
    program->set_mat4("uProj", state.proj);
    if (state.camera) {
        program->set_mat4("uView", state.camera->get_view_matrix());
    }

    // float window_width = 1280;
    // float window_height = 720;
    


    program->set_vec2("uViewport", state.viewport_px);
    program->set_float("uLineWidthPx", width);                 // pick width
    program->set_vec4("uColor", glm::vec4(color.x, color.y, color.z, 1));        // pick color

    // prog->set_mat4("uModel", world);
    // prog->set_mat4("uMVP", mvp);

    // if (state.camera) {
    //     prog->set_vec3("uViewPos", state.camera->position);
    // }


    vao->bind();
    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, instance_count);
    vao->unbind();
}