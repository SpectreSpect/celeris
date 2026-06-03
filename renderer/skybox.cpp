#include "skybox.h"

#include "../vulkan_self/material/material_instance.h"
#include "../vulkan_self/material/material_pass.h"
#include "material_data_types.h"
#include "scene.h"
#include "scene_object.h"
#include "texture_manager.h"

Skybox::Skybox(
    Mesh& mesh,
    MaterialInstance& material,
    TextureManager& texture_manager,
    MaterialPass& pbr_material_pass,
    uint32_t environment_map_id,
    float exposure)
    :   RenderObject(mesh, material),
        m_texture_manager(&texture_manager),
        m_pbr_material_pass(&pbr_material_pass),
        m_environment_map_id(environment_map_id),
        m_exposure(exposure)
{
    update_skybox_material();
}

void Skybox::set_environment_map_id(uint32_t environment_map_id) {
    LOG_METHOD();

    m_environment_map_id = environment_map_id;
    update_skybox_material();
}

uint32_t Skybox::environment_map_id() const noexcept {
    return m_environment_map_id;
}

void Skybox::set_exposure(float exposure) {
    m_exposure = exposure;
    update_skybox_material();
}

float Skybox::exposure() const noexcept {
    return m_exposure;
}

void Skybox::update(Scene& scene) {
    LOG_METHOD();

    logger.check(m_texture_manager != nullptr, "Texture manager pointer is null");
    logger.check(m_pbr_material_pass != nullptr, "PBR material pass pointer is null");

    update_skybox_material();

    for (SceneObject* scene_object : scene.scene_objects) {
        logger.check(scene_object != nullptr, "Scene contains a null object");
        update_object(*scene_object);
    }
}

void Skybox::update_skybox_material() {
    LOG_METHOD();

    logger.check(m_texture_manager != nullptr, "Texture manager pointer is null");
    logger.check(m_environment_map_id < m_texture_manager->hdr_env_maps.size(), "Skybox environment map id is out of range");
    logger.check(m_material != nullptr, "Skybox has no material");

    m_material->descriptor_set.write_cubemap(
        1,
        m_texture_manager->hdr_env_maps[m_environment_map_id]
    );

    set_material_data<SkyboxMaterialData>(SkyboxMaterialData{.exposure = m_exposure});
}

void Skybox::update_object(SceneObject& scene_object) {
    if (RenderObject* render_object = dynamic_cast<RenderObject*>(&scene_object)) {
        if (
            render_object->m_material != nullptr &&
            &render_object->m_material->m_pass == m_pbr_material_pass
        ) {
            render_object->edit_material_data<PBRMaterialData>(
                [&](PBRMaterialData& data) {
                    data.set_pbr_map_id(m_environment_map_id);
                }
            );
        }
    }

    for (SceneObject* child : scene_object.children) {
        logger.check(child != nullptr, "Scene object contains a null child");
        update_object(*child);
    }
}
