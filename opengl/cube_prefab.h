#include "prefab.h"
#include "render_object.h"

class CubePrefab : public Prefab {
public:
    glm::vec3 default_position = {0, 0, 0};
    glm::vec3 albedo = {1, 1, 1};

    SceneObject* instantiate(Scene* scene) override {
        auto* cube = new RenderObject();
        cube->mesh = scene->engine->mesh_manager->get("cube");

        MaterialTemplate* blinn_phong_material_template = scene->engine->material_manager->get("blinn_phong");

        MaterialInstance* material_instance = new MaterialInstance(blinn_phong_material_template);

        // sceneengine->material_manager.get("blinn_phong")

        cube->material = new MaterialInstance(scene->engine->material_manager->get("blinn_phong"));
            
        // );
        // cube->material.set("uAlbedo", albedo);

        // cube->transform.position = default_position;
        scene->add_object(cube);

        return cube;
    }
};
