#include "mcp_visalizer.h"

#include <algorithm>
#include <imgui.h>
#include <imgui_impl_vulkan.h>

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
    m_imgui_texture_sets.reserve(frame_count);
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

    for (VulkanTexture2D& texture : m_visualization_textures) {
        m_imgui_texture_sets.emplace_back(
            engine.device().handle(),
            ImGui_ImplVulkan_AddTexture(
                texture.sampler().handle(),
                texture.view().handle(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
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

MCPVisualizer::~MCPVisualizer() noexcept {
    // Descriptor sets may still be referenced by submitted ImGui draw commands.
    m_engine->device().wait_idle();

    for (const DescriptorSet& descriptor_set : m_imgui_texture_sets) {
        if (descriptor_set.handle() != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RemoveTexture(descriptor_set.handle());
        }
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

void MCPVisualizer::display_interface() {
    const size_t frame = m_engine->current_frame();
    logger().check(frame < m_imgui_texture_sets.size(), "Current frame is out of range");

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 default_size(
        std::min(540.0f, viewport->WorkSize.x - 40.0f),
        std::min(580.0f, viewport->WorkSize.y - 40.0f)
    );
    const ImVec2 default_position(
        viewport->WorkPos.x + viewport->WorkSize.x - default_size.x - 20.0f,
        viewport->WorkPos.y + 20.0f
    );

    ImGui::SetNextWindowPos(default_position, ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(default_size, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(240.0f, 280.0f), viewport->WorkSize);
    ImGui::Begin("MCP Visualization");

    const VkExtent2D texture_extent = m_visualization_textures[frame].extent2d();
    ImVec2 image_size = ImGui::GetContentRegionAvail();

    if (image_size.x > 0.0f && image_size.y > 0.0f && texture_extent.height != 0) {
        const float aspect_ratio =
            static_cast<float>(texture_extent.width) / static_cast<float>(texture_extent.height);

        if (image_size.x / image_size.y > aspect_ratio) {
            image_size.x = image_size.y * aspect_ratio;
        } else {
            image_size.y = image_size.x / aspect_ratio;
        }

        ImGui::Image(
            reinterpret_cast<ImTextureID>(m_imgui_texture_sets[frame].handle()),
            image_size,
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 1.0f)
        );
    }

    ImGui::End();
}

VulkanTexture2D& MCPVisualizer::texture(uint32_t frame_in_flight_id) {
    return m_visualization_textures[frame_in_flight_id];
}
