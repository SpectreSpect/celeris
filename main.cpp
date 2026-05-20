#include "vulkan_self/vulkan_engine.h"
#include "vulkan_self/vulkan_shader_module.h"
#include "vulkan_self/pipeline/vulkan_pipeline_layout.h"
// #include "vulkan_self/vulkan_pipeline.h"
#include "vulkan_self/pipeline/graphics_pipeline.h"
#include "vulkan_self/vulkan_buffer.h"
#include "vulkan_self/vulkan_resource_loader.h"
#include "vulkan_self/descriptor_set/descriptor_set_layout_builder.h"
#include "vulkan_self/descriptor_set/descriptor_set_layout.h"
#include "vulkan_self/descriptor_set/descriptor_pool_builder.h"
#include "vulkan_self/descriptor_set/descriptor_pool.h"
#include "vulkan_self/descriptor_set/descriptor_set.h"
#include "vulkan_self/pipeline/compute_pipeline.h"
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
    queue_request.compute_count = 1;

    VulkanEngine engine(glfw_context, window, queue_request);

    VulkanBuffer unifrom_buffer(
        engine.physical_device(), 
        engine.device(),
        sizeof(SimpleUniform),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VulkanBuffer storage_buffer(
        engine.physical_device(), 
        engine.device(),
        sizeof(SimpleStorage),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    // SimpleStorage simple_storage;
    // simple_storage.color1 = glm::vec4(1, 0, 0, 1);
    // simple_storage.color2 = glm::vec4(0, 0, 1, 1);

    // storage_buffer.upload(&simple_storage, sizeof(SimpleStorage));



    DescriptorSetLayoutBuilder compute_dsl_builder;
    compute_dsl_builder
        .add_uniform_buffer(0, ShaderStages::compute)
        .add_storage_buffer(1, ShaderStages::compute);

    DescriptorSetLayout compute_dsl(engine.device(), compute_dsl_builder);

    DescriptorPoolBuilder compute_pool_builder;
    compute_pool_builder.add_layout(compute_dsl_builder);

    DescriptorPool compute_pool(engine.device(), compute_pool_builder);

    DescriptorSet compute_descriptor_set = compute_pool.allocate_set(compute_dsl);

    PipelineLayoutBuilder compute_pipeline_layout_builder = VulkanPipelineLayout::create_builder();
    compute_pipeline_layout_builder.set_device(engine.device());
    compute_pipeline_layout_builder.add_push_constants<TestPushConstants>();
    compute_pipeline_layout_builder.add_descriptor_set_layout(compute_dsl);
    VulkanPipelineLayout compute_pipeline_layout(compute_pipeline_layout_builder);

    VulkanShaderModule compute_shader(engine.device(), "shaders/test_compute_shader.comp.spv");

    ComputePipeline compute_pipeline(engine.device(), compute_pipeline_layout, compute_shader);


    // Graphics pipeline

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

    GraphicsPipelineBuilder pipeline_builder = GraphicsPipeline::create_builder();
    pipeline_builder.set_graphic_objects(engine.device(), pipeline_layout, engine.swapchain_resources().render_pass);

    VertexLayoutBuilder vertex_layout;
    vertex_layout.add_binding(0, sizeof(SimpleVertex));
    vertex_layout.add_attribute(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SimpleVertex, pos));
    vertex_layout.add_attribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SimpleVertex, color));

    pipeline_builder.set_vertex_layout(vertex_layout);
    pipeline_builder.add_vert_shader_stage(vert_shader_module);
    pipeline_builder.add_frag_shader_stage(frag_shader_module);
    GraphicsPipeline pipeline = GraphicsPipeline(pipeline_builder);

    VulkanCommandBuffer compute_command_buffer(engine.device(), engine.compute_command_pool());
    VulkanFence compute_fence(engine.device());
    {
        auto compute_scope = compute_command_buffer.begin_scope();

        compute_descriptor_set.write_uniform_buffer(0, unifrom_buffer);
        compute_descriptor_set.write_storage_buffer(1, storage_buffer);
        
        compute_pipeline.bind(compute_command_buffer);
        compute_descriptor_set.bind(compute_command_buffer, compute_pipeline);

        compute_command_buffer.dispatch(1, 1, 1);
    }

    engine.device().compute_queue().submit(
        nullptr,
        0,
        compute_command_buffer,
        nullptr,
        &compute_fence
    );
    compute_fence.wait();

    // std::vector<SimpleVertex> vertices = {
    //     {glm::vec2{-0.5f, 0.5f}, glm::vec3{1.0f, 0.0f, 0.0f}},
    //     {glm::vec2{0.5f, 0.5f}, glm::vec3{0.0f, 0.0f, 1.0f}},
    //     {glm::vec2{0.5f, -0.5f}, glm::vec3{0.0f, 1.0f, 0.0f}},

    //     {glm::vec2{-0.5f, 0.5f}, glm::vec3{1.0f, 0.0f, 0.0f}},
    //     {glm::vec2{-0.5f, -0.5f}, glm::vec3{1.0f, 1.0f, 1.0f}},
    //     {glm::vec2{0.5f, -0.5f}, glm::vec3{0.0f, 1.0f, 0.0f}}
    // };
    
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

    VulkanBuffer index_buffer(
        engine.physical_device(), 
        engine.device(),
        indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    // VulkanBuffer storage_buffer(
    //     engine.physical_device(), 
    //     engine.device(),
    //     vertices.size() * sizeof(SimpleStorage),
    //     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    //     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    // );

// <<<<<<< HEAD
//     index_buffer.upload(indices.data(), indices.size() * sizeof(uint32_t));
    
// =======
// >>>>>>> feature/switch-opengl-to-vulcan


    VulkanResourceLoader resource_loader(engine, 1024 * 1024); // 1 Мб
    resource_loader.upload_vertex_buffer(vertices.data(), Utils::size_bytes(vertices), vertex_buffer);
    resource_loader.upload(
        indices.data(),
        Utils::size_bytes(indices),
        index_buffer,
        0,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VK_ACCESS_INDEX_READ_BIT
    );
    resource_loader.upload(
        &simple_storage,
        sizeof(SimpleStorage),
        storage_buffer,
        0,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT
    );
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

                pipeline.bind(command_buffer);
                descriptor_set.bind(command_buffer, pipeline);

                GraphicsPipeline::set_y_up_viewport(
                    command_buffer,
                    engine.swapchain_resources().swapchain.extent()
                );

                GraphicsPipeline::set_scissor(
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
