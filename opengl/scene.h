#pragma once
#include "engine3d.h"
#include "vector"
#include "scene_object.h"
#include "window.h"
#include "render_object.h"

class Scene {
public:
    Engine3D* engine;
    std::vector<SceneObject*> objects;

    Scene(Engine3D* engine) : engine(engine) {}

    void update(float delta_time);
    void draw(Window* window, Camera* camera);


    template<typename T>
    T* add_object(T* obj) {
        objects.push_back(obj);
        return obj;
    }
};
