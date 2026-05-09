#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <iostream>
#include <string>
#include <chrono>
#include <cmath>

#include "vulkan_engine.h"
#include <array>
#include <tuple>


#include "camera.h"
#include "fps_camera_controller.h"

#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <random>
#include "vulkan_window.h"
#include "vulkan/shader_module.h"
#include "vulkan/video_buffer.h"
#include "vulkan/vulkan_vertex_layout.h"
#include "vulkan/graphics_pipeline.h"
#include "vulkan/pbr_renderer.h"
#include "mesh.h"
#include "pbr_uniform.h"
#include "vulkan/image/texture2d.h"
#include "vulkan/render_target_2d.h"
#include "vulkan/render_pass.h"
#include "vulkan/image/cubemap.h"
#include "vulkan/command_pool.h"
#include "vulkan/command_buffer.h"
#include "vulkan/compute_pipeline.h"
#include "vulkan/image/cubemap.h"
#include "math_utils.h"
#include "vulkan/pbr/equirect_to_cubemap_pass.h"

#include "vulkan/resource_loader.h"
#include "skybox/skybox_pass.h"
#include "vulkan/pbr/brdf_lut_generator.h"
#include "vulkan/pbr/irradiance_map_generator.h"
#include "vulkan/pbr/prefilter_map_generator.h"
#include "vulkan/lighting_system/lighting_system.h"
#include "point_cloud/point_cloud_pass.h"
#include "point_cloud/point_cloud.h"
#include "point_cloud/point_cloud_video.h"
#include "icp/gicp.h"
#include "icp/gicp_pass.h"
#include "icp/voxel_point_map.h"
#include "icp/voxel_map_point_inserter.h"
#include "icp/voxel_point_map_reseter.h"
#include <algorithm>
#include "point_cloud/generation/point_cloud_generator.h"

struct Vertex {
    glm::vec4 position;
    glm::vec4 normal;
    glm::vec4 color;
    glm::vec2 uv;
    glm::vec4 tangent;
};

float clear_col[4] = {0, 0, 0, 1};

struct CubePosVertex {
    glm::vec4 position;
};

struct EquirectToCubemapUniform {
    uint32_t image_width;
    uint32_t image_height;
    uint32_t num_layers;
};

Mesh create_cube_mesh(VulkanEngine& engine, glm::vec4 color) {
    std::vector<Vertex> vertices = {
        // +Z (front)
        { glm::vec4(-0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), color, glm::vec2(0.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), color, glm::vec2(1.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), color, glm::vec2(1.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), color, glm::vec2(0.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },

        // -Z (back)
        { glm::vec4( 0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), color, glm::vec2(0.0f, 0.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), color, glm::vec2(1.0f, 0.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), color, glm::vec2(1.0f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), color, glm::vec2(0.0f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 1.0f) },

        // +X (right)
        { glm::vec4( 0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 0.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 0.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 1.0f) },

        // -X (left)
        { glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 0.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 1.0f) },
        { glm::vec4(-0.5f, -0.5f,  0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 0.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f,  0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f, -0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 1.0f) },

        // +Y (top)
        { glm::vec4(-0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },

        // -Y (bottom)
        { glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
    };

    std::vector<uint32_t> indices = {
        // +Z (front)
        0, 1, 2,
        2, 3, 0,

        // -Z (back)
        4, 5, 6,
        6, 7, 4,

        // +X (right)
        8, 9, 10,
        10, 11, 8,

        // -X (left)
        12, 13, 14,
        14, 15, 12,

        // +Y (top)
        16, 17, 18,
        18, 19, 16,

        // -Y (bottom)
        20, 21, 22,
        22, 23, 20
    };

    Mesh mesh(engine,
              vertices.data(),
              sizeof(Vertex) * vertices.size(),
              indices.data(),
              sizeof(uint32_t) * indices.size());

    return mesh;
}

Mesh create_sphere_mesh(VulkanEngine& engine, glm::vec4 color) {
    constexpr float radius = 0.5f;
    constexpr uint32_t stacks = 32;
    constexpr uint32_t sectors = 64;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    vertices.reserve((stacks + 1) * (sectors + 1));
    indices.reserve(stacks * sectors * 6);

    const float pi = 3.14159265358979323846f;
    const float two_pi = 2.0f * pi;

    for (uint32_t i = 0; i <= stacks; ++i) {
        float v = static_cast<float>(i) / static_cast<float>(stacks);
        float phi = v * pi;

        float sin_phi = std::sin(phi);
        float cos_phi = std::cos(phi);

        for (uint32_t j = 0; j <= sectors; ++j) {
            float u = static_cast<float>(j) / static_cast<float>(sectors);
            float theta = u * two_pi;

            float sin_theta = std::sin(theta);
            float cos_theta = std::cos(theta);

            glm::vec3 normal(
                sin_phi * cos_theta,
                cos_phi,
                sin_phi * sin_theta
            );

            glm::vec3 position = radius * normal;

            glm::vec3 tangent;
            if (sin_phi > 1e-6f) {
                tangent = glm::normalize(glm::vec3(
                    -sin_theta,
                    0.0f,
                    cos_theta
                ));
            } else {
                tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            }

            vertices.push_back({
                glm::vec4(position, 1.0f),
                glm::vec4(glm::normalize(normal), 0.0f),
                color,
                glm::vec2(u, v),
                glm::vec4(tangent, 1.0f)
            });
        }
    }

    for (uint32_t i = 0; i < stacks; ++i) {
        for (uint32_t j = 0; j < sectors; ++j) {
            uint32_t k1 = i * (sectors + 1) + j;
            uint32_t k2 = k1 + sectors + 1;

            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }

            if (i != stacks - 1) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    Mesh mesh(
        engine,
        vertices.data(),
        sizeof(Vertex) * vertices.size(),
        indices.data(),
        sizeof(uint32_t) * indices.size()
    );

    return mesh;
}

void print_vec4(glm::vec4 input_vec) {
    std::cout << input_vec.x << " " << input_vec.y << " " << input_vec.z << " " << input_vec.w;
}

#include <glm/gtx/transform.hpp>
#include <algorithm>

static glm::mat4 pose_to_mat4(const glm::vec3& position, const glm::vec3& rotation) {
    glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), rotation.x, glm::vec3(1, 0, 0));
    glm::mat4 Ry = glm::rotate(glm::mat4(1.0f), rotation.y, glm::vec3(0, 1, 0));
    glm::mat4 Rz = glm::rotate(glm::mat4(1.0f), rotation.z, glm::vec3(0, 0, 1));
    glm::mat4 R  = Rz * Ry * Rx;
    glm::mat4 T  = glm::translate(glm::mat4(1.0f), position);
    return T * R;
}

static glm::vec3 mat3_to_euler_xyz_custom(const glm::mat3& R) {
    const float EPS = 1e-6f;
    const float HALF_PI = 1.57079632679f;

    float x, y, z;
    float sy = glm::clamp(-R[0][2], -1.0f, 1.0f);

    if (sy >= 1.0f - EPS) {
        y = HALF_PI;
        z = 0.0f;
        x = std::atan2(R[1][0], R[1][1]);
    }
    else if (sy <= -1.0f + EPS) {
        y = -HALF_PI;
        z = 0.0f;
        x = std::atan2(-R[1][0], R[1][1]);
    }
    else {
        y = std::asin(sy);
        x = std::atan2(R[1][2], R[2][2]);
        z = std::atan2(R[0][1], R[0][0]);
    }

    return glm::vec3(x, y, z);
}

static void mat4_to_pose(const glm::mat4& M, glm::vec3& position, glm::vec3& rotation) {
    position = glm::vec3(M[3]);
    rotation = mat3_to_euler_xyz_custom(glm::mat3(M));
}

static float rotation_angle_from_mat4(const glm::mat4& M) {
    glm::mat3 R(M);
    float trace = R[0][0] + R[1][1] + R[2][2];
    float c = glm::clamp(0.5f * (trace - 1.0f), -1.0f, 1.0f);
    return std::acos(c);
}

glm::vec3 heatmap(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);

    float r = std::clamp(1.5 - std::abs(4.0 * t - 3.0), 0.0, 1.0);
    float g = std::clamp(1.5 - std::abs(4.0 * t - 2.0), 0.0, 1.0);
    float b = std::clamp(1.5 - std::abs(4.0 * t - 1.0), 0.0, 1.0);

    return glm::vec3(r, g, b);
}

static glm::quat rpy_to_quat_zyx(const glm::vec3& rpy) {
    glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), rpy.x, glm::vec3(1, 0, 0));
    glm::mat4 Ry = glm::rotate(glm::mat4(1.0f), rpy.y, glm::vec3(0, 1, 0));
    glm::mat4 Rz = glm::rotate(glm::mat4(1.0f), rpy.z, glm::vec3(0, 0, 1));
    return glm::normalize(glm::quat_cast(glm::mat3(Rz * Ry * Rx)));
}

static glm::vec3 quat_to_rpy_zyx(const glm::quat& q) {
    return PointCloudVideo::mat3_to_rpy_zyx(glm::mat3_cast(glm::normalize(q)));
}

int main() {
    VulkanEngine engine;
    VulkanWindow window(&engine, 1280, 720, "3D visualization");
    engine.set_vulkan_window(&window);

    ui::VulkanInitInfo ui_info{};
    ui_info.instance = engine.instance;
    ui_info.physical_device = engine.physicalDevice;
    ui_info.device = engine.device;
    ui_info.queue_family = engine.get_graphics_queue_family();
    ui_info.queue = engine.graphicsQueue;
    ui_info.descriptor_pool = engine.get_imgui_descriptor_pool();
    ui_info.render_pass = engine.renderPass;
    ui_info.min_image_count = engine.get_imgui_min_image_count();
    ui_info.image_count = static_cast<uint32_t>(engine.swapchainImages.size());
    ui_info.msaa_samples = VK_SAMPLE_COUNT_1_BIT;
    ui_info.pipeline_cache = VK_NULL_HANDLE;
    ui_info.subpass = 0;
    ui_info.check_vk_result_fn = VulkanEngine::imgui_check_vk_result;

    ui::init(window.window, ui_info);


    // VkPhysicalDeviceFeatures supported{};
    // vkGetPhysicalDeviceFeatures(engine.physicalDevice, &supported);

    // if (!supported.shaderFloat64) {
    //     throw std::runtime_error("GPU does not support shaderFloat64");
    // }

    // VkPhysicalDeviceFeatures enabled{};
    // enabled.shaderFloat64 = VK_TRUE;

    // VkDeviceCreateInfo createInfo{};
    // createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    // createInfo.pEnabledFeatures = &enabled;

    // // fill queue infos, extensions, etc.

    // VkDevice device = VK_NULL_HANDLE;
    // VkResult res = vkCreateDevice(engine.physicalDevice, &createInfo, nullptr, &device);
    // if (res != VK_SUCCESS) {
    //     throw std::runtime_error("vkCreateDevice failed");
    // }


    Camera camera = Camera();
    window.set_camera(&camera);

    FPSCameraController camera_controller = FPSCameraController(&camera);
    camera_controller.speed = 20;


    ShaderModule vert_module = ShaderModule(engine.device, "shaders/pbr.vert.spv");
    ShaderModule frag_module = ShaderModule(engine.device, "shaders/pbr.frag.spv");

    ShaderModule equirect_to_cubemap_vs = ShaderModule(engine.device, "shaders/equirect_to_cubemap.vert.spv");
    ShaderModule equirect_to_cubemap_fs = ShaderModule(engine.device, "shaders/equirect_to_cubemap.frag.spv");

    PBRRenderer renderer = PBRRenderer(engine, vert_module, frag_module, 0.3f);

    Mesh mesh = create_sphere_mesh(engine, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    Mesh sphere_2 = create_sphere_mesh(engine, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

    sphere_2.position.x = 2;
    std::vector<Mesh> spheres;

    for (int x = -5; x < 5; x++)
        for (int z = -5; z < 5; z++) {
            // Mesh sphere_mesh = create_sphere_mesh(engine, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

            // sphere_mesh.position = glm::vec3(x, 0, z);

            spheres.push_back(create_sphere_mesh(engine, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)));
            spheres.back().position = glm::vec3(x, 0, z);
        }

    
    EquirectToCubemapPass equirect_to_cubemap_pass(engine);
    BrdfLutGenerater brdf_lut_generator(engine);
    IrradianceMapGenerator irradiance_map_generator(engine);
    PrefilterMapGenerator prefilter_map_generator(engine);

    ResourceLoader resource_loader;
    resource_loader.create(engine, 154217728);

    // Texture2D equirectangular_map = resource_loader.load_hdr_texture2d("assets/hdr/st_peters_square_night_4k.hdr", 6, VK_IMAGE_USAGE_SAMPLED_BIT);
    // Texture2D equirectangular_map = resource_loader.load_hdr_texture2d("assets/hdr/citrus_orchard_puresky_4k.hdr", 6, VK_IMAGE_USAGE_SAMPLED_BIT);
    Texture2D equirectangular_map = resource_loader.load_hdr_texture2d("assets/hdr/studio_kominka_02_4k.hdr", 6, VK_IMAGE_USAGE_SAMPLED_BIT);
    
    Texture2D brdf_lut = brdf_lut_generator.generate(256, 256);
    
    Cubemap environment_map = equirect_to_cubemap_pass.generate(equirectangular_map, 1920);
    Cubemap irradiance_map = irradiance_map_generator.generate(environment_map, 256);
    Cubemap prefilter_map = prefilter_map_generator.generate(environment_map, 256);

    SkyboxPass skybox_pass;
    skybox_pass.create(engine);

    LightingSystem lighting_system;
    lighting_system.init(engine);

    LightSource red_light_source{glm::vec4(1, 2, 0, 10), glm::vec4(1, 0, 0, 1)};
    LightSource green_light_source{glm::vec4(0, 2, 1, 10), glm::vec4(0, 1, 0, 1)};
    LightSource blue_light_source{glm::vec4(-1, 2, 0, 10), glm::vec4(0, 0, 1, 1)};

    // light_source.position = glm::vec4(0, 2, 0, 10);
    // light_source.color = glm::vec4(1, 0, 0, 1);

    lighting_system.set_light_source(0, red_light_source);
    lighting_system.set_light_source(1, green_light_source);
    lighting_system.set_light_source(2, blue_light_source);

    PointCloudPass point_cloud_pass;
    point_cloud_pass.create(engine);


    PointCloudGenerator point_cloud_generator;
    point_cloud_generator.create(engine);

    // PointCloud generated_point_cloud = point_cloud_generator.generate(glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), 0, glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), 0);
    PointCloud generated_point_cloud;
    generated_point_cloud.create(engine, 3600 * 16);

    float pi = glm::pi<float>();

    uint32_t num_point_cloud_frames = 100;
    PointCloudFrame point_cloud_frames[num_point_cloud_frames];
    // PointCloud point_cloud_frames[num_point_cloud_frames];
    // VideoBuffer normal_buffers[num_point_cloud_frames];

    const uint32_t forward_frames = 10;
    const uint32_t turn_frames    = 20;
    const uint32_t return_frames  = num_point_cloud_frames - forward_frames - turn_frames;

    const float start_x     = 0.0f;
    const float end_x       = 1.0f;   // "end of road"
    const float scanner_y   = 0.3f;

    for (uint32_t i = 0; i < num_point_cloud_frames; i++) {
        glm::vec3 position(0.0f);
        glm::vec3 rotation(0.0f);

        if (i < forward_frames) {
            float t = float(i) / float(forward_frames - 1);

            // Move from start_x -> end_x
            float x = glm::mix(start_x, end_x, t);
            position = glm::vec3(x, scanner_y, 0.0f);

            // Facing forward
            rotation = glm::vec3(0.0f, 0.0f, 0.0f);
        }
        else if (i < forward_frames + turn_frames) {
            float t = float(i - forward_frames) / float(turn_frames - 1);

            // Stay at the end of the road
            position = glm::vec3(end_x, scanner_y, 0.0f);

            // Turn from 0 -> 180 degrees
            float yaw = glm::mix(0.0f, pi, t);
            rotation = glm::vec3(0.0f, yaw, 0.0f);
        }
        else {
            float t = float(i - forward_frames - turn_frames) / float(return_frames - 1);

            // Move from end_x -> start_x
            float x = glm::mix(end_x, start_x, t);
            position = glm::vec3(x, scanner_y, 0.0f);

            // Facing backward
            rotation = glm::vec3(0.0f, pi, 0.0f);
        }

        uint32_t num_points = 3600 * 16;

        point_cloud_frames[i].point_cloud.create(engine, num_points);
        point_cloud_frames[i].normal_buffer.create(engine, num_points * sizeof(glm::vec4));

        point_cloud_generator.generate(point_cloud_frames[i].point_cloud, point_cloud_frames[i].normal_buffer, position, rotation);

        // point_cloud_frames[i].point_cloud.position = position;
        // point_cloud_frames[i].point_cloud.rotation = rotation;

        // point_cloud_frames[i] = point_cloud_generator.generate(
        //     position,
        //     rotation,
        //     0,
        //     glm::vec3(0, 0, 0),
        //     glm::vec3(0, 0, 0),
        //     0
        // );
    }




    // PointCloudVideo point_cloud_video = PointCloudVideo();
    // point_cloud_video.load_from_file(engine, "/home/spectre/TEMP_lidar_output_mesh/recording/index.csv", 247);

    // PointCloudFrame& source_point_cloud_frame = point_cloud_video.frames[89];
    // PointCloudFrame& target_point_cloud_frame = point_cloud_video.frames[95];

    // source_point_cloud_frame.point_cloud.position = glm::vec3(0, 0, 0);
    // source_point_cloud_frame.point_cloud.rotation = glm::vec3(0, 0, 0);

    // for (int i = 0; i < source_point_cloud_frame.points.size(); i++) {
    //     source_point_cloud_frame.points[i].color = glm::vec4(1, 0, 0, 1);
    // }
    // source_point_cloud_frame.point_cloud.set_points(source_point_cloud_frame.points);

    VoxelPointMap voxel_point_map;
    voxel_point_map.create(engine, 1500000, 1500000);

    VoxelPointMapReseter voxel_point_map_reseter;
    voxel_point_map_reseter.create(engine);

    voxel_point_map_reseter.reset(voxel_point_map);

    VoxelMapPointInserter voxel_map_point_inserter;
    voxel_map_point_inserter.create(engine);

    // target_point_cloud_frame.point_cloud.position = glm::vec3(0, 0, 0);
    // target_point_cloud_frame.point_cloud.rotation = glm::vec3(0.0f, 0, 0);

    voxel_map_point_inserter.insert(voxel_point_map, point_cloud_frames[0].point_cloud, point_cloud_frames[0].normal_buffer);

    GICPPass gicp_pass;
    gicp_pass.create(engine);

    PointCloud voxel_map_point_cloud;
    voxel_map_point_cloud.create(engine, voxel_point_map.map_point_count);

    voxel_map_point_cloud.set_points(voxel_point_map.map_point_buffer, voxel_point_map.map_point_count);


    // PointCloud original_scan;
    // original_scan.create(engine);
    // original_scan.color = glm::vec4(1, 0, 0, 1);

    // PointCloud fitted_scan;
    // original_scan.create(engine);
    // fitted_scan.color = glm::vec4(0, 0, 1, 1);

    // std::vector<glm::vec3> raw_positions(point_cloud_video.frames.size());
    // std::vector<glm::vec3> raw_rotations(point_cloud_video.frames.size());
    // std::vector<glm::mat4> raw_world_poses(point_cloud_video.frames.size(), glm::mat4(1.0f));
    // std::vector<glm::mat4> relative_deltas(point_cloud_video.frames.size(), glm::mat4(1.0f));

    // for (size_t i = 0; i < point_cloud_video.frames.size(); ++i) {
    //     raw_positions[i] = point_cloud_video.frames[i].point_cloud.position;
    //     raw_rotations[i] = point_cloud_video.frames[i].point_cloud.rotation;
    //     raw_world_poses[i] = pose_to_mat4(raw_positions[i], raw_rotations[i]);
    // }

    // for (size_t i = 1; i < point_cloud_video.frames.size(); ++i) {
    //     relative_deltas[i] = glm::inverse(raw_world_poses[i - 1]) * raw_world_poses[i];
    // }

    // std::vector<glm::quat> predicted_orientation(
    //     point_cloud_video.frames.size(),
    //     glm::quat(1.0f, 0.0f, 0.0f, 0.0f)
    // );

    // point_cloud_video.current_frame = 5;

    // std::vector<glm::quat> predicted_q(
    //     point_cloud_video.frames.size(),
    //     glm::quat(1.0f, 0.0f, 0.0f, 0.0f)
    // );


    // predicted_q[point_cloud_video.current_frame] =
    //     rpy_to_quat_zyx(point_cloud_video.frames[point_cloud_video.current_frame].point_cloud.rotation);

    // point_cloud_video.frames[point_cloud_video.current_frame].velocity = glm::vec3(0.0f);
    // point_cloud_video.frames[point_cloud_video.current_frame].angular_velocity = glm::vec3(0.0f);

    // glm::vec3 gravity = point_cloud_video.frames[point_cloud_video.current_frame].linear_acceleration;



    // for (size_t i = point_cloud_video.current_frame + 1; i < point_cloud_video.frames.size(); ++i) {
    //     const auto& prev = point_cloud_video.frames[i - 1];
    //     auto& curr = point_cloud_video.frames[i];

    //     float dt = float(curr.timestamp_ns - prev.timestamp_ns) * 1e-9f;
    //     if (dt <= 0.0f) {
    //         curr.velocity = prev.velocity;
    //         predicted_q[i] = predicted_q[i - 1];
    //         continue;
    //     }

    //     // Linear motion
    //     glm::vec3 a = prev.linear_acceleration - gravity;
    //     curr.velocity = prev.velocity + a * dt;

    //     // Angular motion: angular_velocity is already measured in rad/s
    //     glm::vec3 omega0 = prev.angular_velocity;
    //     glm::vec3 omega1 = curr.angular_velocity;
    //     glm::vec3 omega_avg = 0.5f * (omega0 + omega1);

    //     float omega_len = glm::length(omega_avg);
    //     if (omega_len > 1e-6f) {
    //         glm::vec3 axis = omega_avg / omega_len;
    //         float angle = omega_len * dt;

    //         glm::quat dq = glm::angleAxis(angle, axis);

    //         // angular velocity is in BODY/LOCAL frame -> right multiply
    //         predicted_q[i] = glm::normalize(predicted_q[i - 1] * dq);
    //     } else {
    //         predicted_q[i] = predicted_q[i - 1];
    //     }

    //     curr.point_cloud.rotation = quat_to_rpy_zyx(predicted_q[i]);
    //     curr.point_cloud.position = glm::vec3(0, 0, 0);
    // }

    // for (int i = 0; i < 10; i++) {
    //     GICP::step(source_point_cloud_frame, target_point_cloud_frame, source_point_cloud_frame.normals, target_point_cloud_frame.normals);
    // }

    // size_t last_frame_id = point_cloud_video.current_frame;
    size_t last_frame_id = 0;
    float timer = 0.0f;
    float last_frame = 0.0f;
    float start_time = (float)glfwGetTime();
    while (window.is_open()) {
        engine.poll_events();

        float currentFrame = (float)glfwGetTime() - start_time;
        float delta_time = currentFrame - last_frame;
        last_frame = currentFrame;

        camera_controller.update(&window, delta_time);

        red_light_source.position.y = sin(timer * 2 + 0.234f) * 2;
        green_light_source.position.x = sin(timer * 2 + 1.3423f) * 2;
        blue_light_source.position.z = cos(timer * 2) * 2;

        engine.begin_frame(glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));
        if (!engine.frameInProgress) {
            continue;
        }

        // skybox_pass.render(camera, environment_map);

        ui::begin_frame();
        ui::update_mouse_mode(&window);

        lighting_system.update(camera);

        
        point_cloud_pass.render(point_cloud_frames[last_frame_id].point_cloud, camera);

        // glm::vec3 f = glm::normalize(camera.front);
        // float rotY = std::atan2(-f.z, std::sqrt(f.x * f.x + f.y * f.y));
        // float rotZ = std::atan2(f.y, f.x);
        // float rotX = 0.0f;
        // point_cloud_generator.generate(generated_point_cloud, camera.position, glm::vec3(rotX, rotY, rotZ), 0, glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), 0);
        // point_cloud_pass.render(generated_point_cloud, camera);

        // point_cloud_pass.render(source_point_cloud_frame.point_cloud, camera);
        // point_cloud_pass.render(target_point_cloud_frame.point_cloud, camera);
        point_cloud_pass.render(voxel_map_point_cloud, camera);

        ImGui::Begin("Hello");

        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        // ImGui::Text("Current video frame: %d", point_cloud_video.current_frame);

        if (ImGui::Button("GICP step")) {
            // GICP::step(source_point_cloud_frame, target_point_cloud_frame, source_point_cloud_frame.normals, target_point_cloud_frame.normals);
            // GICP::step_test(source_point_cloud_frame, target_point_cloud_frame, source_point_cloud_frame.normals, target_point_cloud_frame.normals);
            // gicp_pass.step(source_point_cloud_frame.point_cloud, target_point_cloud_frame.point_cloud, source_normal_buffer, target_normal_buffer);
            // gicp_pass.step(voxel_point_map, source_point_cloud_frame.point_cloud, source_point_cloud_frame.normal_buffer);
            // gicp_pass.fit(voxel_point_map, source_point_cloud_frame.point_cloud, source_point_cloud_frame.normal_buffer, 10);

            // std::cout << "position: (";
            // print_vec4(glm::vec4(source_point_cloud_frame.point_cloud.position, 1.0f));
            // std::cout << ")     rotation: (";
            // print_vec4(glm::vec4(source_point_cloud_frame.point_cloud.rotation, 1.0f));
            // std::cout << ")" << std::endl;
        }


        if (ImGui::Button("Next frame")) {
            last_frame_id++;

            point_cloud_frames[last_frame_id].point_cloud.position = point_cloud_frames[last_frame_id - 1].point_cloud.position;
            point_cloud_frames[last_frame_id].point_cloud.rotation = point_cloud_frames[last_frame_id - 1].point_cloud.rotation;

            gicp_pass.fit(voxel_point_map, point_cloud_frames[last_frame_id].point_cloud, point_cloud_frames[last_frame_id].normal_buffer, 10);
            
            voxel_map_point_inserter.insert(voxel_point_map, point_cloud_frames[last_frame_id].point_cloud, point_cloud_frames[last_frame_id].normal_buffer);
            
            voxel_map_point_cloud.set_points(voxel_point_map.map_point_buffer, voxel_point_map.map_point_count);

            // point_cloud_video.current_frame += 1;
            // const auto& rot = point_cloud_video.frames[point_cloud_video.current_frame].point_cloud.rotation;

            // std::cout << "(" << rot.x << ", " << rot.y << ", " << rot.z << ")" << std::endl;
        }

        ImGui::End();

        ui::end_frame(engine.currentCommandBuffer);
        engine.end_frame();

        timer += delta_time;
    }

    vkDeviceWaitIdle(engine.device);
    ui::shutdown();
}

