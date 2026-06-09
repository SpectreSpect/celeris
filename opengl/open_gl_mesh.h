#pragma once
#include "vao.h"
#include "vbo.h"
#include "ebo.h"
#include "vertex_layout.h"
#include "program.h"
#include "drawable.h"
#include "transformable.h"

class SSBO;

class Mesh : public Drawable, public Transformable {
public:
    VAO* vao;
    VBO* vbo;
    EBO* ebo;
    VertexLayout* vertex_layout;
    
    Mesh() = default;
    Mesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, VertexLayout* vertex_layout);
    Mesh(const SSBO& vertex_buffer, const SSBO& index_buffer, VertexLayout* vertex_layout);
    ~Mesh();
    // Mesh(const void* vertex_data, size_t vertex_data_size, const void* index_data, size_t index_data_size, VertexLayout* vertex_layout);
    void update(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, GLenum usage = GL_DYNAMIC_DRAW);
    void update(const void* vertex_data, size_t vertex_data_size, const void* index_data, size_t index_data_size, GLenum usage = GL_DYNAMIC_DRAW);
    
    // Mesh(float* vertices, unsigned int* indices, int vertices_size, int indices_size, Program* shader, VertexLayout* vertex_layout);
    void draw(RenderState state) override;
};