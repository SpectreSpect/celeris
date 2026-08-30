#pragma once

#include <vector>

#include "../../vulkan_self/pass/instance/slot_pass_instance.h"
#include "../../vulkan_self/image/vulkan_texture_2d.h"
#include "../../vulkan_self/descriptor_set/descriptor_set.h"
#include "../scene_object.h"
#include "../render_object.h"

class TextureManager;
class MaterialManager;
class VulkanEngine;
class MeshManager;

class MCPVisualizer : public SceneObject {
public:
    MCPVisualizer(
        VulkanEngine& engine,
        TextureManager& texture_manager,
        MaterialManager& material_manager,
        MeshManager& mesh_manager,
        uint32_t texture_width, 
        uint32_t texture_height,
        float skybox_exposure
    );
    ~MCPVisualizer() noexcept;

    void update(glm::vec3 target_position);
    void display_interface();
    VulkanTexture2D& texture(uint32_t frame_in_flight_id);
private:
    VulkanEngine* m_engine = nullptr;
    TextureManager* m_texture_manager = nullptr;

    std::vector<VulkanTexture2D> m_visualization_textures;
    std::vector<DescriptorSet> m_imgui_texture_sets;
    std::vector<SlotPassInstance> m_pass_instances;
    std::vector<RenderObject> m_quads;
};
