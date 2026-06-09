#include "scene.h"

void Scene::add(SceneObject& scene_object) {
    scene_objects.push_back(&scene_object);
}