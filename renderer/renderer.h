#pragma once

#include <vector>
#include <glm/glm.hpp>


class VulkanEngine;
class FrameResources;
class VulkanCommandBuffer;
class InstancedRenderObject;
class TransformPushConstants;
class RenderObject;
class InstancedRenderObject;
class Scene;

class Renderer {
public:
    Renderer(VulkanEngine& engine, FrameResources& frame_resources);

    void render(VulkanCommandBuffer& command_buffer, RenderObject& render_object, glm::mat4 transform = glm::mat4(1.0f));
    void render(VulkanCommandBuffer& command_buffer, InstancedRenderObject& render_object, glm::mat4 transform = glm::mat4(1.0f));

    // void render(VulkanCommandBuffer& command_buffer, RenderObject& render_object);
    // void render(VulkanCommandBuffer& command_buffer, InstancedRenderObject& render_object);

    void render(VulkanCommandBuffer& command_buffer, std::vector<RenderObject*> render_objects, glm::mat4 transform = glm::mat4(1.0f));
    
    void render(VulkanCommandBuffer& command_buffer, Scene& scene);

private:
    VulkanEngine* m_engine = nullptr;
    FrameResources* m_frame_resources = nullptr;
};