#include "circle_cloud.h"
#include "engine3d.h"

CircleCloud::CircleCloud() { 

    vertex_layout = new VertexLayout();
    vertex_layout->add("position", 0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), 0, 0, {0.0f, 0.0f, 0.0f});
    vertex_layout->add("normal", 1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), 3 * sizeof(float), 0, {0.0f, 1.0f, 0.0f});
    vertex_layout->add("color", 2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), 6 * sizeof(float), 0, {1.0f, 1.0f, 1.0f});

    constexpr float PI = 3.14159265358979323846f;

    int angle_count = 32;
    float radius = 1.0f;

    angle_count = std::max(angle_count, 3);

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    // Center vertex
    vertices.insert(vertices.end(), {
        0.0f, 0.0f, 0.0f,   // position
        0.0f, 1.0f, 0.0f,   // normal
        1.0f, 1.0f, 1.0f    // color
    });

    // Rim vertices
    for (int i = 0; i < angle_count; ++i) {
        float angle = 2.0f * PI * float(i) / float(angle_count);
        float x = radius * std::cos(angle);
        float z = radius * std::sin(angle);

        vertices.insert(vertices.end(), {
            x,    0.0f, z,   // position
            0.0f, 1.0f, 0.0f, // normal
            1.0f, 1.0f, 1.0f  // color
        });
    }

    // Triangle fan indices
    for (int i = 0; i < angle_count; ++i) {
        unsigned int current = 1 + i;
        unsigned int next    = 1 + ((i + 1) % angle_count);

        indices.push_back(0);
        indices.push_back(next);
        indices.push_back(current);
    }

    num_indices = indices.size();
    
    CirlceInstance dummy{};
    dummy.position_radius   = {0, 0, 0, 1};
    dummy.normal = {0, 1, 0, 1};
    dummy.color = {1, 1, 1, 1};

    instance_vbo = new VBO(&dummy, sizeof(CirlceInstance));

    instance_layout = new VertexLayout();
    instance_layout->add("position_radius", 3, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), 0, 1, {0.0f, 0.0f, 0.0f, 1.0f});
    instance_layout->add("normal", 4, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), 4 * sizeof(float), 1, {0.0f, 1.0f, 0.0f, 1.0f});
    instance_layout->add("color", 5, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), 8 * sizeof(float), 1, {1.0f, 1.0f, 1.0f, 1.0f});

    mesh = new Mesh(vertices, indices, vertex_layout);

    mesh->vao->bind();

    mesh->vbo->bind();
    vertex_layout->apply();

    instance_vbo->bind();
    instance_layout->apply();

    mesh->ebo->bind();
    mesh->vao->unbind();
}

void CircleCloud::set_instances(const std::vector<CirlceInstance>& instances) {
    instance_count = (int)instances.size();
    if (instance_count == 0) return;

    instance_vbo->update_mapped(
        instances.data(),
        instances.size() * sizeof(CirlceInstance),
        GL_STREAM_DRAW
    );
}

void CircleCloud::draw(RenderState state) {
    if (instance_count <= 0) return;

    Program* program = state.engine->default_circle_program;
    program->use();

    // --- Apply Transformable like Mesh does ---
    glm::mat4 model = get_model_matrix();
    glm::mat4 world = state.transform * model;
    glm::mat4 mvp = state.vp * world;

    // Your point shader must use this (see snippet below)
    // program->set_mat4("uModel", world);

    // program->set_mat4("uProj", state.proj);
    // if (state.camera) {
    //     program->set_mat4("uView", state.camera->get_view_matrix());
    // }
    
    // program->set_vec2("uViewport", state.viewport_px);

    // program->set_int("uScreenSpaceSize", constant_screen_size ? 1 : 0);
    // program->set_float("uPointSizePx", size_px);
    // program->set_float("uPointSizeWorld", size_world);
    // program->set_int("uRound", round ? 1 : 0);

    program->set_mat4("uModel", world);
    program->set_mat4("uProj", state.proj);


    state.engine->lighting_system.bind_buffers();
    program->set_uint("xTiles", state.engine->lighting_system.num_clusters.x);
    program->set_uint("yTiles", state.engine->lighting_system.num_clusters.y);
    program->set_uint("zSlices", state.engine->lighting_system.num_clusters.z);
    program->set_float("screenWidth", state.viewport_px.x);
    program->set_float("screenHeight", state.viewport_px.y);
    program->set_uint("max_lights_per_cluster", state.engine->lighting_system.max_lights_per_cluster);
    if (state.camera) {
        program->set_vec3("uViewPos", state.camera->position);
        program->set_mat4("uView", state.camera->get_view_matrix());
        program->set_float("nearPlane", state.camera->near_plane);
        program->set_float("farPlane", state.camera->far_plane);
    }

    mesh->vao->bind();
    glDrawElementsInstanced(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, 0, instance_count);
    mesh->vao->unbind();
}