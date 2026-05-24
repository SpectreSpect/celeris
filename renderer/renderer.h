#pragma once

class VulkanEngine;
class FrameResources;
class VulkanCommandBuffer;
class InstancedRenderObject;
class TransformPushConstants;
class RenderObject;
class InstancedRenderObject;

class Renderer {
public:
    Renderer(VulkanEngine& engine, FrameResources& frame_resources);

    void render(VulkanCommandBuffer& command_buffer, RenderObject& render_object);
    void render(VulkanCommandBuffer& command_buffer, InstancedRenderObject& render_object);

private:
    VulkanEngine* m_engine = nullptr;
    FrameResources* m_frame_resources = nullptr;
};