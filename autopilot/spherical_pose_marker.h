#pragma once

#include "../renderer/scene_object.h"
#include "../renderer/render_object.h"

class MeshManager;
class MaterialInstanceManager;
class PBRMaterialData;

class SphericalPoseMarker : public SceneObject {
public:
    SphericalPoseMarker(MeshManager& mesh_manager, 
                        MaterialInstanceManager& material_instance_manager,
                        PBRMaterialData material_data);

private:
    RenderObject m_main_sphere;
    RenderObject m_direction_sphere;
};