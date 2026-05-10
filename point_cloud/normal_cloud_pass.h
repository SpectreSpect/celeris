#pragma once

#include "../vulkan/renderer.h"
#include "../vulkan/graphics_pipeline.h"
#include "../vulkan/shader_module.h"
#include "../mesh.h"
#include "point_cloud.h"
#include "point_instance.h"
#include "../camera.h"


class NormalCloudPass {
public:
    struct NormalLineVertex {
        glm::vec2 corner;
    };

    struct NormalCloudUniform {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec2 viewport;
    };

    struct NormalCloudPushConstants {
        glm::vec4 color;
        glm::mat4 model;

        float normal_length;
        float line_width_px;
        int use_point_color;
        int pad_0;
    };

    VulkanEngine* engine = nullptr;

    ShaderModule vertex_shader;
    ShaderModule fragment_shader;
    GraphicsPipeline pipeline;

    VideoBuffer uniform_buffer;
    DescriptorSetBundle descriptor_set_bundle;

    Mesh line_mesh;

    float normal_length = 0.5f;
    float line_width_px = 2.0f;
    bool use_point_color = true;

    glm::vec4 color = glm::vec4(0.2f, 0.8f, 1.0f, 1.0f);

    NormalCloudPass() = default;

    void create(VulkanEngine& engine);
    void render(PointCloud& point_cloud, VideoBuffer& normal_buffer, Camera& camera);
};