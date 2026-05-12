#include "vulkan_engine.h"

#include <fstream>
#include <array>

namespace {
    std::vector<char> read_file(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        logger.check(file.is_open(), "Failed to open file: " + filename);

        size_t file_size = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(file_size);

        file.seekg(0);
        file.read(buffer.data(), file_size);
        file.close();

        return buffer;
    }
}

VulkanEngine::VulkanEngine(
    const GlfwContext& glfw_context,
    Window& window,
    const QueueRequest& queue_request,
    std::string_view app_name)
    :   m_window(window), 
        m_instance(glfw_context, app_name),
        m_surface(m_instance, m_window),
        m_physical_device(m_instance, m_surface, queue_request),
        m_device(m_physical_device),
        m_swapchain_resources(std::in_place, m_physical_device, m_device, m_surface, m_window),
        m_command_pool(m_device, m_device.graphics_queue()),
        m_command_buffers(
            VulkanCommandBuffer::create_command_buffers(
                m_device, 
                m_command_pool, 
                static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)
            )
        ),
        m_in_flight_fences(VulkanFence::create_fences(m_device, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT))),
        m_image_available_semaphores(VulkanSemaphore::create_semaphores(m_device, MAX_FRAMES_IN_FLIGHT))
    {
        create_graphics_pipeline();
    }

VulkanEngine::~VulkanEngine() {
    cleanup_graphics_pipeline();
}

void VulkanEngine::run() {
    LOG_METHOD();

    while (!m_window.should_close()) {
        m_window.poll_events();
        draw_frame();
    }

    m_device.wait_idle();
}

void VulkanEngine::record_command_buffer(VulkanCommandBuffer& command_buffer, uint32_t image_index) {
    LOG_METHOD();
    {
        auto command_buffer_scope = command_buffer.begin_scope();
        {
            auto render_pass_scope = m_swapchain_resources->render_pass.begin_scope(
                command_buffer,
                m_swapchain_resources->framebuffers[image_index],
                m_swapchain_resources->swapchain,
                {{0.05f, 0.08f, 0.12f, 1.0f}}
            );

            vkCmdBindPipeline(
                command_buffer.handle(),
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_graphics_pipeline
            );

            vkCmdDraw(
                command_buffer.handle(),
                3,
                1,
                0,
                0
            );
        }
    }
}

void VulkanEngine::draw_frame() {
    LOG_METHOD();

    m_in_flight_fences[m_current_frame].wait();

    uint32_t image_index = 0;
    VkResult result = m_swapchain_resources->swapchain.acquire_next_image(image_index, m_image_available_semaphores[m_current_frame]);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return;
    }

    logger.check(
        result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR,
        "Failed to acquire next image"
    );
    
    m_in_flight_fences[m_current_frame].reset();
    m_command_buffers[m_current_frame].reset();
    record_command_buffer(m_command_buffers[m_current_frame], image_index);

    m_device.graphics_queue().submit(
        m_image_available_semaphores[m_current_frame],
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        m_command_buffers[m_current_frame],
        m_swapchain_resources->render_finished_semaphores[image_index],
        m_in_flight_fences[m_current_frame]
    );

    result = m_device.present_queue().present(
        m_swapchain_resources->render_finished_semaphores[image_index],
        m_swapchain_resources->swapchain,
        image_index
    );
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR ||
        result == VK_SUBOPTIMAL_KHR ||
        m_window.is_window_resized)
    {
        recreate_swapchain();
    } else {
        logger.check(result == VK_SUCCESS, "Failed to present image");
    }

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanEngine::recreate_swapchain() {
    LOG_METHOD();

    m_window.is_window_resized = false;
    m_window.wait_until_framebuffer_available(); // Ждём, чтобы окно было развёрнуто

    m_device.wait_idle();

    m_swapchain_resources.reset();
    m_swapchain_resources.emplace(m_physical_device, m_device, m_surface, m_window);
}

VkShaderModule VulkanEngine::create_shader_module(const std::vector<char>& code) {
    LOG_METHOD();

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shader_module = VK_NULL_HANDLE;

    VkResult result = vkCreateShaderModule(
        m_device.handle(),
        &create_info,
        nullptr,
        &shader_module
    );

    logger.check(result == VK_SUCCESS, "Failed to create shader module");

    return shader_module;
}

void VulkanEngine::create_graphics_pipeline() {
    auto vert_shader_code = read_file("shaders/triangle.vert.spv");
    auto frag_shader_code = read_file("shaders/triangle.frag.spv");

    VkShaderModule vert_shader_module = create_shader_module(vert_shader_code);
    VkShaderModule frag_shader_module = create_shader_module(frag_shader_code);

    VkPipelineShaderStageCreateInfo vert_shader_stage_info{};
    vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = vert_shader_module;
    vert_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
    frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = frag_shader_module;
    frag_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        vert_shader_stage_info,
        frag_shader_stage_info
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.pVertexBindingDescriptions = nullptr;
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    vertex_input_info.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapchain_resources->swapchain.extent().width);
    viewport.height = static_cast<float>(m_swapchain_resources->swapchain.extent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapchain_resources->swapchain.extent();

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    color_blending.blendConstants[0] = 0.0f;
    color_blending.blendConstants[1] = 0.0f;
    color_blending.blendConstants[2] = 0.0f;
    color_blending.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 0;
    pipeline_layout_info.pSetLayouts = nullptr;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = nullptr;

    VkResult result = vkCreatePipelineLayout(
        m_device.handle(),
        &pipeline_layout_info,
        nullptr,
        &m_pipeline_layout
    );

    logger.check(result == VK_SUCCESS, "Failed to create pipeline layout");

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = nullptr;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = nullptr;
    pipeline_info.layout = m_pipeline_layout;
    pipeline_info.renderPass = m_swapchain_resources->render_pass.handle();
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    result = vkCreateGraphicsPipelines(
        m_device.handle(),
        VK_NULL_HANDLE,
        1,
        &pipeline_info,
        nullptr,
        &m_graphics_pipeline
    );

    logger.check(result == VK_SUCCESS, "Failed to create graphics pipeline");

    vkDestroyShaderModule(m_device.handle(), frag_shader_module, nullptr);
    vkDestroyShaderModule(m_device.handle(), vert_shader_module, nullptr);
}

void VulkanEngine::cleanup_graphics_pipeline() {
    if (m_graphics_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device.handle(), m_graphics_pipeline, nullptr);
        m_graphics_pipeline = VK_NULL_HANDLE;
    }

    if (m_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device.handle(), m_pipeline_layout, nullptr);
        m_pipeline_layout = VK_NULL_HANDLE;
    }
}
