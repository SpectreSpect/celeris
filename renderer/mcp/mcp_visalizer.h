#pragma once

#include "../../vulkan_self/pass/instance/slot_pass_instance.h"
#include "../../vulkan_self/image/vulkan_texture_2d.h"
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

    void update(glm::vec3 target_position);
private:
    TextureManager* m_texture_manager = nullptr;

    VulkanTexture2D m_visualization_texture;
    SlotPassInstance m_pass_instance;
    RenderObject quad;
};