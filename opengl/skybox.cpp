#include "skybox.h"
#include "engine3d.h"

Skybox::Skybox(const Cubemap &cubemap) {
// Skybox::Skybox() {
    this->cubemap = &cubemap;

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
}


void Skybox::draw(RenderState state) {
    Program* program = state.engine->skybox_program;

    program->use();

    if (state.camera) {
        program->set_mat4("uView", state.camera->get_view_matrix());
        program->set_mat4("uProj", state.proj);
    }

    cubemap->bind(0);
    program->set_int("uSkybox", 0);

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    // glDisable(GL_CULL_FACE);
    // or: glEnable(GL_CULL_FACE); glCullFace(GL_FRONT);

    mesh->vao->bind();
    glDrawElements(GL_TRIANGLES, mesh->ebo->num_indices, GL_UNSIGNED_INT, 0);
    mesh->vao->unbind();

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    // restore this only if your engine normally expects culling enabled
    // glEnable(GL_CULL_FACE);
    // glCullFace(GL_BACK);
}