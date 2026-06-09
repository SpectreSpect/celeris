#pragma once
#include <glm/glm.hpp>
#include "camera.h"

struct Program; // forward

class Engine3D;

struct RenderState {
    glm::mat4 proj;
    glm::mat4 vp;       // projection * view
    glm::mat4 transform;    // accumulated parent->world transform
    Program* program;   // shader program to use (optional)
    Camera* camera;
    Engine3D* engine;

    glm::ivec2 viewport_px;
};
