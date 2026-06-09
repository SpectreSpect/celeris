#pragma once
#include <glm/glm.hpp>
#include "camera.h"
#include "vulkan/shader_module.h"
// #include "renderer.h"

class Renderer;
class VulkanEngine;
class LightingSystem;

struct RenderState {
    glm::mat4 proj;
    glm::mat4 vp;       // projection * view
    glm::mat4 view;       // projection * view
    glm::mat4 transform;    // accumulated parent->world transform
    uint32_t prefilte_map_mip_levels;
    VulkanShaderModule* vertex_shader;
    VulkanShaderModule* fragment_shader;
    Camera* camera;
    VulkanEngine* engine;
    Renderer* renderer;
    LightingSystem* lighting_system = nullptr;

    glm::ivec2 viewport_px;
};
