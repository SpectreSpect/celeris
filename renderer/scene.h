#pragma once
#include "render_object.h"

class Scene {
public:
    std::vector<SceneObject*> scene_objects;

    Scene() = default;

    void add(SceneObject& scene_object);
};