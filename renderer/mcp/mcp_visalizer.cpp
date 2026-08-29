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
:   m_engine(&engine),
    m_texture_manager(&texture_manager) {
    const uint32_t frame_count = engine.num_frames_in_flight();

    m_visualization_textures.reserve(frame_count);
    m_pass_instances.reserve(frame_count);
    m_quads.reserve(frame_count);

    for (uint32_t frame = 0; frame < frame_count; ++frame) {
        m_visualization_textures.emplace_back(
            texture_manager.mcp_visualization_texture_pass.generate(
                texture_width,
                texture_height
            )
        );
    }

    for (uint32_t frame = 0; frame < frame_count; ++frame) {
        m_pass_instances.emplace_back(material_manager.create_pbr_material(
            engine,
            texture_manager.irradiance_maps,
            texture_manager.prefilter_maps,
            texture_manager.brdf_lut,
            m_visualization_textures[frame]
        ));
    }

    for (uint32_t frame = 0; frame < frame_count; ++frame) {
        m_quads.emplace_back(mesh_manager.quad, m_pass_instances[frame]);
        m_quads.back().set_material_data(
            PBRMaterialData::create(
                0,
                1.0f,
                skybox_exposure,
                glm::vec4(1.0f)
            )
        );
        m_quads.back().visible = false;
        add_child(m_quads.back());
    }
}

void MCPVisualizer::update(glm::vec3 target_position) {
    const size_t frame = m_engine->current_frame();

    logger().check(frame < m_visualization_textures.size(), "Current frame is out of range");

    for (RenderObject& quad : m_quads) {
        quad.visible = false;
    }
    m_quads[frame].visible = true;

    m_texture_manager->mcp_visualization_texture_pass.render(
        m_visualization_textures[frame],
        transform.get_model_matrix() * m_quads[frame].transform.get_model_matrix(),
        target_position,
        5.0f
    );
}
