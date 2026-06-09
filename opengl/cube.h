#pragma once
#include "mesh.h"
#include "mesh_data.h"

class Cube : public Drawable, public Transformable {
public:
    Mesh mesh;

    Cube(glm::vec3 position = glm::vec3(0.0f), glm::vec3 scale = glm::vec3(1.0f), glm::vec3 color = glm::vec3(1.0f));
    const glm::vec3& get_color() const { return _color; }
    void set_color(glm::vec3& color);

    static MeshData create_mesh_data(glm::vec3 color);
    static Mesh create_mesh(glm::vec3 color);

    void draw(RenderState state) override;
    
private:
    glm::vec3 _color;
};