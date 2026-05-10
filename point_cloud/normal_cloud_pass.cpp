#include "normal_cloud_pass.h"


void NormalCloudPass::create(VulkanEngine& engine) {
    this->engine = &engine;

    vertex_shader = ShaderModule(engine.device, "shaders/normal_cloud/normal_cloud.vert.spv");
    fragment_shader = ShaderModule(engine.device, "shaders/normal_cloud/normal_cloud.frag.spv");

    VulkanVertexLayout vertex_layout;
    LayoutInitializer layout_initializer = vertex_layout.get_initializer();

    layout_initializer.add_binding(
        VK_VERTEX_INPUT_RATE_VERTEX,
        sizeof(NormalLineVertex)
    );

    layout_initializer.add(AttrFormat::FLOAT2, 0); // aCorner

    uniform_buffer = VideoBuffer(engine, sizeof(NormalCloudUniform));

    DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();

    builder.add_uniform_buffer(
        0,
        uniform_buffer,
        VK_SHADER_STAGE_VERTEX_BIT
    );

    builder.add_storage_buffer(
        1,
        VK_SHADER_STAGE_VERTEX_BIT
    ); // PointInstance buffer

    builder.add_storage_buffer(
        2,
        VK_SHADER_STAGE_VERTEX_BIT
    ); // Normal vec4 buffer

    descriptor_set_bundle = builder.create(engine.device);

    pipeline = GraphicsPipeline(
        engine,
        descriptor_set_bundle,
        vertex_layout,
        vertex_shader,
        fragment_shader
    );

    const NormalLineVertex line_vertices[] = {
        {{-1.0f, 0.0f}},
        {{+1.0f, 0.0f}},
        {{-1.0f, 1.0f}},
        {{+1.0f, 1.0f}},
    };

    const unsigned int line_indices[] = {
        0, 1, 2,
        2, 1, 3
    };

    line_mesh = Mesh(
        engine,
        (void*)line_vertices,
        (uint32_t)sizeof(line_vertices),
        (unsigned int*)line_indices,
        (uint32_t)sizeof(line_indices)
    );
}


void NormalCloudPass::render(
    PointCloud& point_cloud,
    VideoBuffer& normal_buffer,
    Camera& camera
) {
    if (!engine) {
        throw std::runtime_error("NormalCloudPass::render: engine was null");
    }

    if (point_cloud.num_instances <= 0) {
        return;
    }

    NormalCloudUniform uniform_data{};

    float aspect =
        float(engine->swapchainExtent.width) /
        float(engine->swapchainExtent.height);

    uniform_data.view = camera.get_view_matrix();
    uniform_data.proj = camera.get_projection_matrix(aspect);
    uniform_data.viewport = glm::vec2(
        float(engine->swapchainExtent.width),
        float(engine->swapchainExtent.height)
    );

    uniform_buffer.update_data(&uniform_data, sizeof(uniform_data));

    descriptor_set_bundle.bind_storage_buffer(
        1,
        point_cloud.get_instance_buffer()
    );

    descriptor_set_bundle.bind_storage_buffer(
        2,
        normal_buffer
    );

    engine->bind_pipeline(pipeline);

    engine->bind_vertex_buffer(line_mesh.vertex_buffer);
    engine->bind_index_buffer(line_mesh.index_buffer);

    NormalCloudPushConstants pc{};

    // pc.color = color;
    pc.color = point_cloud.color;
    pc.model = point_cloud.get_model_matrix();
    pc.normal_length = normal_length;
    pc.line_width_px = line_width_px;
    pc.use_point_color = use_point_color ? 1 : 0;
    pc.pad_0 = 0;

    vkCmdPushConstants(
        engine->currentCommandBuffer,
        pipeline.pipeline_layout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(pc),
        &pc
    );

    vkCmdDrawIndexed(
        engine->currentCommandBuffer,
        line_mesh.num_indices,
        point_cloud.num_instances,
        0,
        0,
        0
    );
}