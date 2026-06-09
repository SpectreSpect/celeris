#pragma once
#include <glm/glm.hpp>
// #include "camera.h"
#include "vulkan/shader_module.h"
#include "vulkan/graphics_pipeline.h"

class VulkanEngine;

struct RenderState {
    glm::mat4 proj;
    glm::mat4 vp;       // projection * view
    glm::mat4 transform;    // accumulated parent->world transform
    VulkanShaderModule* vertex_shader;
    VulkanShaderModule* fragment_shader;
    // Camera* camera;
    VulkanEngine* engine;
    GraphicsPipeline* graphics_pipeline;

    glm::ivec2 viewport_px;
};
