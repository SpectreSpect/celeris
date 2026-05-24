#pragma once
#include "render_object.h"

class Scene {
public:
    std::vector<RenderObject*> render_objects;

    Scene() = default;

    void add_object(RenderObject& render_object);
};