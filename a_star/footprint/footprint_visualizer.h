#pragma once

#include "footprint.h"

#include "../../renderer/render_object.h"
#include "../../renderer/scene_object.h"

#include <vector>

namespace celeris {
    class Celeris;
}
class MaterialInstanceManager;
class MeshManager;

class FootprintVisualizer : public SceneObject {
public:
    FootprintVisualizer(
        MeshManager& mesh_manager,
        MaterialInstanceManager& material_instance_manager,
        celeris::Celeris& celeris,
        Footprint& footprint,
        float skybox_exposure
    );

    void update_footprint(const NonholonomicPos& rear_axle_pos);

private:
    Footprint* m_footprint = nullptr;
    std::vector<RenderObject> m_markers;
};
