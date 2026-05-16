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

    struct LidarTransformFrame {
        uint32_t frame_index;
        glm::vec3 position;
        glm::vec3 rotation; // Euler XYZ in radians: pitch, yaw, roll
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
    static glm::vec3 safe_normalize(glm::vec3 v, glm::vec3 fallback) ;
    static glm::vec3 euler_xyz_from_mat3(const glm::mat3& R);
    static glm::vec3 lidar_rotation_from_camera_front_up(glm::vec3 camera_front, glm::vec3 camera_up_hint);
    static glm::vec3 lidar_rotation_from_camera(const Camera& camera);
    static void write_camera_transform_header(std::ofstream& out) ;
    static void write_camera_transform(std::ofstream& out, uint32_t frame_index, const Camera& camera);
    static void save_camera_transformations(const std::string& path, const std::vector<Camera>& cameras);
    static std::vector<LidarTransformFrame> load_lidar_transformations(const std::string& path);
    uint32_t generate_from_camera_transform_file(PointCloudFrame* point_cloud_frames, uint32_t max_point_cloud_frames,
                                                                      int width, int height, const std::string& path);

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