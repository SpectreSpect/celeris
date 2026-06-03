#pragma once

#include <cstdint>

#include "../vulkan_self/logger/logger_header.h"
#include "render_object.h"

class MaterialPass;
class Scene;
class SceneObject;
class TextureManager;

class Skybox : public RenderObject {
public:
    _XCHILD_NAME(Skybox);

    Skybox(
        Mesh& mesh,
        MaterialInstance& material,
        TextureManager& texture_manager,
        MaterialPass& pbr_material_pass,
        uint32_t environment_map_id,
        float exposure = 1.0f
    );

    void set_environment_map_id(uint32_t environment_map_id);
    uint32_t environment_map_id() const noexcept;

    void set_exposure(float exposure);
    float exposure() const noexcept;

    void update(Scene& scene);

private:
    void update_skybox_material();
    void update_object(SceneObject& scene_object);

private:
    TextureManager* m_texture_manager = nullptr;
    MaterialPass* m_pbr_material_pass = nullptr;
    uint32_t m_environment_map_id = 0;
    float m_exposure = 1.0f;
};
