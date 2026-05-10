#pragma once
#include "../../vulkan/renderer.h"
#include "../../vulkan/graphics_pipeline.h"
#include "../../vulkan/shader_module.h"
#include "../../vulkan/image/cubemap.h"
#include "../../vulkan/lighting_system/lighting_system.h"
#include "../../mesh.h"
#include "../point_cloud.h"
#include "../point_instance.h"
#include "../../camera.h"
#include "../point_cloud_frame.h"


class PointCloudGenerator {
public:
    struct PointCloudGeneratorUniform {
        uint32_t width;
        uint32_t height;
        float max_range;
        uint32_t pad_0;
        glm::vec4 position;
        glm::vec4 rotation;
    };

    // struct PointCloudGeneratorPushConstants {
    //     glm::vec4 color;
    //     glm::mat4 model;
    //     float point_size_px;
    //     float point_size_world;
    //     int screen_space_size;
    // };
    // uint32_t num_instances = 0;

    PointCloudGenerator() = default;
    void create(VulkanEngine& engine);
    PointCloud generate(glm::vec3 position, glm::vec3 rotation, float time, 
                        glm::vec3 prev_position, glm::vec3 prev_rotation, float prev_time);
    
    void generate(PointCloud& point_cloud, VideoBuffer& normal_buffer, glm::vec3 position, glm::vec3 rotation, int width, int height);
    void generate_with_motion(PointCloudFrame* point_cloud_frames, uint32_t num_point_cloud_frames, int width, int height);

    // 1. points
    // 2. normals
    // 
    // void render(PointCloud& point_cloud, Camera& camera);
    CommandPool command_pool;
    CommandBuffer command_buffer;
private:
    VulkanEngine* engine = nullptr;
    ComputePipeline pipeline;
    DescriptorSetBundle descriptor_set_bundle;
    Fence fence;
    VideoBuffer uniform_buffer;
    ShaderModule compute_shader;
    
    uint32_t compute_queue_family_id;
    VkQueue compute_queue;
    VideoBuffer point_instance_buffer;
};