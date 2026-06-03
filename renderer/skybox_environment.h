#pragma once

#include <cstdint>

#include "../vulkan_self/logger/logger_header.h"

class MaterialPass;
class RenderObject;
class Scene;
class SceneObject;

class SkyboxEnvironment {
public:
    _XCLASS_NAME(SkyboxEnvironment);

    SkyboxEnvironment(RenderObject& skybox_object, MaterialPass& pbr_material_pass, uint32_t pbr_map_id);

    void set_pbr_map_id(uint32_t pbr_map_id) noexcept;
    uint32_t pbr_map_id() const noexcept;

    void update(Scene& scene) const;

private:
    void update_object(SceneObject& scene_object) const;

private:
    RenderObject* m_skybox_object = nullptr;
    MaterialPass* m_pbr_material_pass = nullptr;
    uint32_t m_pbr_map_id = 0;
};
