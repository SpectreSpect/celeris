#include "scene.h"

void Scene::add_object(RenderObject& render_object) {
    render_objects.push_back(&render_object);
}