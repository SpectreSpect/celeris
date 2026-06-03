#include "skybox_environment.h"

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/material/material_instance.h"
#include "../vulkan_self/material/material_pass.h"
#include "material_data_types.h"
#include "render_object.h"
#include "scene.h"
#include "scene_object.h"

SkyboxEnvironment::SkyboxEnvironment(
    RenderObject& skybox_object,
    MaterialPass& pbr_material_pass,
    uint32_t pbr_map_id)
    :   m_skybox_object(&skybox_object),
        m_pbr_material_pass(&pbr_material_pass),
        m_pbr_map_id(pbr_map_id) {}

void SkyboxEnvironment::set_pbr_map_id(uint32_t pbr_map_id) noexcept {
    m_pbr_map_id = pbr_map_id;
}

uint32_t SkyboxEnvironment::pbr_map_id() const noexcept {
    return m_pbr_map_id;
}

void SkyboxEnvironment::update(Scene& scene) const {
    LOG_METHOD();

    logger.check(m_skybox_object != nullptr, "Skybox object pointer is null");
    logger.check(m_pbr_material_pass != nullptr, "PBR material pass pointer is null");

    for (SceneObject* scene_object : scene.scene_objects) {
        logger.check(scene_object != nullptr, "Scene contains a null object");
        update_object(*scene_object);
    }
}

void SkyboxEnvironment::update_object(SceneObject& scene_object) const {
    if (RenderObject* render_object = dynamic_cast<RenderObject*>(&scene_object)) {
        if (
            render_object->m_material != nullptr &&
            &render_object->m_material->m_pass == m_pbr_material_pass
        ) {
            render_object->edit_material_data<PBRMaterialData>(
                [&](PBRMaterialData& data) {
                    data.pbr_map_ids.x = m_pbr_map_id;
                    data.pbr_map_ids.y = m_pbr_map_id;
                }
            );
        }
    }

    for (SceneObject* child : scene_object.children) {
        logger.check(child != nullptr, "Scene object contains a null child");
        update_object(*child);
    }
}
