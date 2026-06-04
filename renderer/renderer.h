#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "../vulkan_self/logger/logger_header.h"


class VulkanEngine;
class FrameResources;
class VulkanCommandBuffer;
class InstancedRenderObject;
class TransformPushConstants;
class SceneObject;
class RenderObject;
class VulkanBuffer;

class InstancedRenderObject;
class Scene;

class Renderer {
public:
    _XCLASS_NAME(Renderer);

    Renderer(VulkanEngine& engine, FrameResources& frame_resources);

    void render(VulkanCommandBuffer& command_buffer, RenderObject& render_object, glm::mat4 transform = glm::mat4(1.0f));
    void render_indirect(
        VulkanCommandBuffer& command_buffer,
        RenderObject& render_object,
        VulkanBuffer& indirect_buffer,
        uint32_t draw_count,
        glm::mat4 transform = glm::mat4(1.0f)
    );
    void render(VulkanCommandBuffer& command_buffer, InstancedRenderObject& render_object, glm::mat4 transform = glm::mat4(1.0f));

    // void render(VulkanCommandBuffer& command_buffer, RenderObject& render_object);
    // void render(VulkanCommandBuffer& command_buffer, InstancedRenderObject& render_object);

    void render(VulkanCommandBuffer& command_buffer, std::vector<SceneObject*> scene_objects, glm::mat4 transform = glm::mat4(1.0f));
    
    void render(VulkanCommandBuffer& command_buffer, Scene& scene);

private:
    VulkanEngine* m_engine = nullptr;
    FrameResources* m_frame_resources = nullptr;
};
