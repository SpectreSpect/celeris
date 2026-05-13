#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../vulkan/image/cubemap.h"
#include "../vulkan_engine.h"
#include "../vulkan/command_buffer.h"
#include "../vulkan/command_pool.h"
#include "../vulkan/graphics_pipeline.h"
#include "../mesh.h"
#include "../camera.h"
#include "../path_utils.h"

class SkyboxPass {
public:
    struct SkyboxUniform {
        glm::mat4 proj;
        glm::mat4 view;
        float exposure;
    };

    float exposure = 0.3f;

    SkyboxPass() = default;
    void create(VulkanEngine& engine, float exposure = 0.3f);
    void render(Camera& camera, Cubemap& environment_map);
    void render(glm::mat4 projection_matrix, glm::mat4 view_matrix, Cubemap& environment_map);
private:
    VulkanEngine* engine;
    CommandPool command_pool;
    CommandBuffer command_buffer;

    VulkanShaderModule skybox_vs;
    VulkanShaderModule skybox_fs;

    DescriptorSetBundle descriptor_set_bundle;
    GraphicsPipeline pipeline;
    
    VideoBuffer uniform_buffer;

    Mesh cube_mesh;

    uint32_t compute_queue_family_id;
    VkQueue compute_queue;
};