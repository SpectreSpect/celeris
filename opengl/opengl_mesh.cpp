#include "mesh.h"
#include "ssbo.h"
#include "engine3d.h"

Mesh::Mesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, VertexLayout* vertex_layout) {
    vao = new VAO();
    vbo = new VBO(vertices.data(), vertices.size() * sizeof(float));
    ebo = new EBO(indices.data(), indices.size() * sizeof(unsigned int));

    vao->init_vao();

    this->vertex_layout = vertex_layout;

    vao->setup(*vbo, *ebo, *vertex_layout);
}

Mesh::Mesh(const SSBO& vertex_buffer, const SSBO& index_buffer, VertexLayout* vertex_layout) {
    vao = new VAO();
    
    // vbo = new VBO(vertex_buffer.id_, vertex_buffer.size_bytes());
    // ebo = new EBO(index_buffer.id_, index_buffer.size_bytes());
    vbo = new VBO(vertex_buffer);
    ebo = new EBO(index_buffer);

    vao->init_vao();

    this->vertex_layout = vertex_layout;

    vao->setup(*vbo, *ebo, *vertex_layout);
}

Mesh::~Mesh() {
    delete vao;
    delete vbo;
    delete ebo;
}

void Mesh::update(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, GLenum usage) {
    if (vbo)
        vbo->update_mapped(vertices.data(), vertices.size() * sizeof(float), usage);
    
    if (ebo)
        ebo->update_mapped(indices.data(), indices.size() * sizeof(unsigned int), usage);
}

void Mesh::update(const void* vertex_data, size_t vertex_data_size, const void* index_data, size_t index_data_size, GLenum usage) {
    if (vbo)
        vbo->update_mapped(vertex_data, vertex_data_size, usage);
    
    if (ebo)
        ebo->update_mapped(index_data, index_data_size, usage);
}

void Mesh::draw(RenderState state) {
    glm::mat4 model = get_model_matrix();
    glm::mat4 world = state.transform * model;
    glm::mat4 mvp = state.vp * world;

    Program* prog = state.program;
    prog->use();
    
    prog->set_mat4("uModel", world);
    prog->set_mat4("uMVP", mvp);


    // uniform uint xTiles;
    // uniform uint yTiles;
    // uniform uint zSlices;
    // uniform float screenWidth;
    // uniform float screenHeight;
    // uniform float nearPlane;
    // uniform float farPlane;

    state.engine->lighting_system.bind_buffers();
    prog->set_uint("xTiles", state.engine->lighting_system.num_clusters.x);
    prog->set_uint("yTiles", state.engine->lighting_system.num_clusters.y);
    prog->set_uint("zSlices", state.engine->lighting_system.num_clusters.z);
    prog->set_float("screenWidth", state.viewport_px.x);
    prog->set_float("screenHeight", state.viewport_px.y);
    prog->set_uint("max_lights_per_cluster", state.engine->lighting_system.max_lights_per_cluster);
    if (state.camera) {
        prog->set_vec3("uViewPos", state.camera->position);
        prog->set_mat4("uView", state.camera->get_view_matrix());
        prog->set_float("nearPlane", state.camera->near_plane);
        prog->set_float("farPlane", state.camera->far_plane);
    }

    vao->bind();
    glDrawElements(GL_TRIANGLES, ebo->num_indices, GL_UNSIGNED_INT, 0);
    vao->unbind();
}
