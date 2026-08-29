#include "mcp_visalizer.h"

#include "../../vulkan_self/vulkan_engine.h"
#include "../../managers/material_manager.h"
#include "../../managers/texture_manager.h"
#include "../../managers/mesh_manager.h"
#include "../material_data_types.h"

MCPVisualizer::MCPVisualizer(
    VulkanEngine& engine,
    TextureManager& texture_manager,
    MaterialManager& material_manager,
    MeshManager& mesh_manager,
    uint32_t texture_width, 
    uint32_t texture_height,
    float skybox_exposure)
:   m_texture_manager(&texture_manager),
    m_visualization_texture(texture_manager.mcp_visualization_texture_pass.generate(
        texture_width,
        texture_height
    )),
    m_pass_instance(material_manager.create_pbr_material(
        engine, 
        texture_manager.irradiance_maps, 
        texture_manager.prefilter_maps, 
        texture_manager.brdf_lut,
        m_visualization_texture
    )),
    quad(mesh_manager.quad, m_pass_instance) {
    quad.set_material_data(PBRMaterialData::create(0, 1.0f, skybox_exposure, glm::vec4(1, 1, 1, 1)));

    add_child(quad);
}

void MCPVisualizer::update(glm::vec3 target_position) {
    m_texture_manager->mcp_visualization_texture_pass.render(
        m_visualization_texture,
        transform.get_model_matrix() * quad.transform.get_model_matrix(),
        target_position,
        5.0f
    );
}
