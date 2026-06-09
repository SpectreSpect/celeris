#include "quad.h"


Quad::Quad() {
    static std::vector<float> quadVertices = {
        -1.0f, -1.0f,
        1.0f, -1.0f,
        1.0f,  1.0f,
        -1.0f,  1.0f
    };

    static std::vector<unsigned int> quadIndices = {
        0, 1, 2,
        0, 2, 3
    };

    VertexLayout* quadLayout = new VertexLayout();
    quadLayout->add("position", 0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0, 0, {0.0f, 0.0f});

    mesh = new Mesh(quadVertices, quadIndices, quadLayout);
}