#include "vulkan_self/vulkan_engine.h"
#include "vulkan_self/vulkan_shader_module.h"
#include "vulkan_self/vulkan_pipeline_layout.h"
#include "vulkan_self/vulkan_pipeline.h"
#include "vulkan_self/vulkan_buffer.h"
#include "vulkan_self/vulkan_resource_loader.h"
#include "vulkan_self/descriptor_set/descriptor_set_layout_builder.h"
#include "vulkan_self/descriptor_set/descriptor_set_layout.h"
#include "vulkan_self/descriptor_set/descriptor_pool_builder.h"
#include "vulkan_self/descriptor_set/descriptor_pool.h"
#include "vulkan_self/descriptor_set/descriptor_set.h"
#include "vulkan_self/vulkan_image.h"
#include "vulkan_self/image/cpu_image.h"
#include "path_utils.h"

#include <vector>

struct TestPushConstants {
    glm::vec2 offset;
    float scale;
};

struct SimpleVertex {
    glm::vec2 pos;
    glm::vec3 color;
};

struct SimpleUniform {
    glm::vec4 color;
};

struct SimpleStorage {
    glm::vec4 color1;
    glm::vec4 color2;
};

int main() {
    GlfwContext glfw_context;
    Window window(glfw_context, 1280, 720, "Vulkan engine");

    QueueRequest queue_request;
    queue_request.graphics_count = 1;
    queue_request.present_count = 1;

    VulkanEngine engine(glfw_context, window, queue_request);

    DescriptorSetLayoutBuilder dsl_builder;
    dsl_builder
        .add_uniform_buffer(0, ShaderStages::fragment)
        .add_storage_buffer(1, ShaderStages::fragment);

    DescriptorSetLayout dsl(engine.device(), dsl_builder);

    DescriptorPoolBuilder pool_builder;
    pool_builder.add_layout(dsl_builder);

    DescriptorPool pool(engine.device(), pool_builder);

    DescriptorSet descriptor_set = pool.allocate_set(dsl);

    PipelineLayoutBuilder pipeline_layout_builder = VulkanPipelineLayout::create_builder();
    pipeline_layout_builder.set_device(engine.device());
    pipeline_layout_builder.add_push_constants<TestPushConstants>();
    pipeline_layout_builder.add_descriptor_set_layout(dsl);
    VulkanPipelineLayout pipeline_layout(pipeline_layout_builder);

    VulkanShaderModule vert_shader_module(engine.device(), path_utils::executable_dir() / "shaders" / "triangle.vert.spv");
    VulkanShaderModule frag_shader_module(engine.device(), path_utils::executable_dir() / "shaders" / "triangle.frag.spv");

    PipelineBuilder pipeline_builder = VulkanPipeline::create_builder();
    pipeline_builder.set_graphic_objects(engine.device(), pipeline_layout, engine.swapchain_resources().render_pass);

    VertexLayoutBuilder vertex_layout;
    vertex_layout.add_binding(0, sizeof(SimpleVertex));
    vertex_layout.add_attribute(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SimpleVertex, pos));
    vertex_layout.add_attribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SimpleVertex, color));

    pipeline_builder.set_vertex_layout(vertex_layout);
    pipeline_builder.add_vert_shader_stage(vert_shader_module);
    pipeline_builder.add_frag_shader_stage(frag_shader_module);
    VulkanPipeline pipeline = VulkanPipeline(pipeline_builder);
    
    std::vector<SimpleVertex> vertices = {
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // bottom-left
        {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, // bottom-right
        {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, // top-right
        {{-0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}}, // top-left
    };

    std::vector<uint32_t> indices = {
        0, 1, 2,
        2, 3, 0
    };

    SimpleStorage simple_storage;
    simple_storage.color1 = glm::vec4(1, 0, 0, 1);
    simple_storage.color2 = glm::vec4(0, 0, 1, 1);

    VulkanBuffer vertex_buffer = VulkanBuffer::create_vertex_buffer(engine, Utils::size_bytes(vertices));
    VulkanBuffer index_buffer = VulkanBuffer::create_index_buffer(engine, Utils::size_bytes(indices));
    VulkanBuffer storage_buffer = VulkanBuffer::create_storage_buffer(engine, sizeof(SimpleStorage));
    VulkanBuffer unifrom_buffer = VulkanBuffer::create_host_visible_uniform_buffer(engine, vertices.size() * sizeof(SimpleUniform));

    CpuImage cpu_image = CpuImage::load_rgba8_image(
        path_utils::executable_dir() / "assets" / "textures" / "minecraft_dirt" / "texture.png"
    );

    VulkanImage texture_image(
        engine.physical_device(),
        engine.device(),
        VkExtent3D{16, 16, 1},
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    VulkanResourceLoader resource_loader(engine, 1024 * 1024); // 1 Мб
    resource_loader.upload_vertex_buffer(vertices.data(), Utils::size_bytes(vertices), vertex_buffer);
    resource_loader.upload_index_buffer(indices.data(), Utils::size_bytes(indices), index_buffer);
    resource_loader.upload_compute_storage_buffer(&simple_storage, sizeof(SimpleStorage), storage_buffer);
    resource_loader.upload_sampled_image_2d(cpu_image.image_data().data(), cpu_image.extent2d(), texture_image);
    resource_loader.submit();

    descriptor_set.write_uniform_buffer(0, unifrom_buffer);
    descriptor_set.write_storage_buffer(1, storage_buffer);

    while (!engine.window().should_close()) {
        engine.window().poll_events();
        
        uint32_t image_index = 0;
        if (!engine.aquire_free_resources(image_index)) continue;
        VulkanCommandBuffer& command_buffer = engine.get_active_command_buffer();

        // Запись команд
        {auto command_buffer_scope = command_buffer.begin_scope();
            {auto render_pass_scope = engine.swapchain_resources().render_pass.begin_scope(
                command_buffer,
                engine.swapchain_resources().framebuffers[image_index],
                engine.swapchain_resources().swapchain,
                {{0.05f, 0.08f, 0.12f, 1.0f}}
            );

                pipeline.bind(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS);
                descriptor_set.bind(command_buffer, pipeline_layout);

                VulkanPipeline::set_y_up_viewport(
                    command_buffer,
                    engine.swapchain_resources().swapchain.extent()
                );

                VulkanPipeline::set_scissor(
                    command_buffer,
                    engine.swapchain_resources().swapchain.extent()
                );

                vertex_buffer.bind_as_vertex_buffer(command_buffer);
                index_buffer.bind_as_index_buffer(command_buffer);

                static TestPushConstants pc{
                    .offset = {0.0f, 0.0f},
                    .scale = 1.0f
                };

                pc.offset.x += 0.001f;
                
                pipeline_layout.push_constants(command_buffer, pc);

                SimpleUniform ubo;
                ubo.color = glm::vec4(pc.offset.x, 0.0f, 1.0f, 1.0f);
                unifrom_buffer.upload(&ubo, sizeof(SimpleUniform));

                vkCmdDrawIndexed(command_buffer.handle(), indices.size(), 1, 0, 0, 0);
            }
        }

        engine.submit_graphic_commands(image_index);
        engine.present(image_index);
    }

    engine.device().wait_idle();
}
