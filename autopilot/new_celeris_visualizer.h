#pragma once

#include "../vulkan_self/logger/logger_header.h"
#include "../renderer/scene_object.h"
#include "gazelle_next.h"

class VehicleGeometry;
class MeshManager;
class NewCeleris;

class NewCelerisVisualizer : public SceneObject {
public:
    _XCLASS_NAME(NewCelerisVisualizer);

    NewCelerisVisualizer(
        MeshManager& mesh_manager,
        MaterialInstanceManager& material_instance_manager, 
        NewCeleris& celeris, 
        const VehicleGeometry& vehicle_geometry,
        float skybox_exposure = 1.8f
    );

    void update();

    void gazelle_next_visible(bool visible);
    void voxel_grid_visible(bool visible);

private:
    NewCeleris* m_celeris = nullptr;

    GazelleNext m_gazelle_next;

    void update_gazelle_next_transform();
};