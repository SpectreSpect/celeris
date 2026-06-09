#include "skybox_pass.h"

void SkyboxPass::create(VulkanEngine& engine, float exposure) {
    this->exposure = exposure;
    this->engine = &engine;

    compute_queue_family_id = vulkan_utils::find_compute_queue_family(engine.physicalDevice);
    vkGetDeviceQueue(engine.device, compute_queue_family_id, 0, &compute_queue);

    command_pool.create(engine.device, engine.physicalDevice, compute_queue_family_id, compute_queue);
    command_buffer.create(command_pool);

    skybox_vs.create(engine.device, executable_dir() / "shaders" / "skybox.vert.spv");
    skybox_fs.create(engine.device, executable_dir() / "shaders" / "skybox.frag.spv");

    VulkanVertexLayout vertex_layout;
    LayoutInitializer layout_initializer = vertex_layout.get_initializer();
    layout_initializer.add(AttrFormat::FLOAT4); // position

    uniform_buffer = VideoBuffer(engine, sizeof(SkyboxUniform));

    DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    // builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_VERTEX_BIT);
    builder.add_combined_image_sampler(1, VK_SHADER_STAGE_FRAGMENT_BIT); // env_map
    descriptor_set_bundle = builder.create(engine.device);

    pipeline = GraphicsPipeline(engine, descriptor_set_bundle, vertex_layout, skybox_vs, skybox_fs, false);

    

    std::vector<glm::vec4> vertices = {
        glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f), // 0
        glm::vec4( 0.5f, -0.5f, -0.5f, 1.0f), // 1
        glm::vec4( 0.5f,  0.5f, -0.5f, 1.0f), // 2
        glm::vec4(-0.5f,  0.5f, -0.5f, 1.0f), // 3
        glm::vec4(-0.5f, -0.5f,  0.5f, 1.0f), // 4
        glm::vec4( 0.5f, -0.5f,  0.5f, 1.0f), // 5
        glm::vec4( 0.5f,  0.5f,  0.5f, 1.0f), // 6
        glm::vec4(-0.5f,  0.5f,  0.5f, 1.0f)  // 7
    };

    std::vector<uint32_t> indices = {
        // +Z (front)
        4, 5, 6,
        6, 7, 4,

        // -Z (back)
        1, 0, 3,
        3, 2, 1,

        // +X (right)
        5, 1, 2,
        2, 6, 5,

        // -X (left)
        0, 4, 7,
        7, 3, 0,

        // +Y (top)
        7, 6, 2,
        2, 3, 7,

        // -Y (bottom)
        0, 1, 5,
        5, 4, 0
    };

    cube_mesh = Mesh(engine, vertices.data(), sizeof(glm::vec4) * vertices.size(), indices.data(), sizeof(uint32_t) * indices.size());

    // uniform_buffer = VideoBuffer(engine, sizeof(PBRUniform));

    // DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    // builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    // builder.add_combined_image_sampler(1, VK_SHADER_STAGE_FRAGMENT_BIT); // albedo
    // builder.add_combined_image_sampler(2, VK_SHADER_STAGE_FRAGMENT_BIT); // env_map
    // descriptor_set_bundle = builder.create(engine.device);


    // equirect_to_cubemap_uniform_buffer.create(engine, sizeof(EquirectToCubemapUniform));



    // DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    // builder.add_uniform_buffer(0, equirect_to_cubemap_uniform_buffer, VK_SHADER_STAGE_COMPUTE_BIT);
    // builder.add_combined_image_sampler(1, VK_SHADER_STAGE_COMPUTE_BIT);
    // builder.add_image_storage(2, VK_SHADER_STAGE_COMPUTE_BIT);
    // descriptor_set_bundle = builder.create(engine.device);

    // pipeline.create(engine.device, descriptor_set_bundle, equirect_to_cubemap_cs);

    // fence = Fence(engine.device);
}

void SkyboxPass::render(Camera& camera, Cubemap& environment_map) {
    float aspect = float(engine->swapchainExtent.width) / float(engine->swapchainExtent.height);
    render(camera.get_projection_matrix(aspect), camera.get_view_matrix(), environment_map);
}

void SkyboxPass::render(glm::mat4 projection_matrix, glm::mat4 view_matrix, Cubemap& environment_map) {
    if (!engine)
        throw "engine was null";


    SkyboxUniform ubo{};

    ubo.proj = projection_matrix;
    ubo.view = view_matrix;
    ubo.exposure = exposure;

    uniform_buffer.update_data(&ubo, sizeof(ubo));

    descriptor_set_bundle.bind_combined_image_sampler(1, environment_map);
    
    engine->bind_pipeline(pipeline);
    engine->bind_vertex_buffer(cube_mesh.vertex_buffer);
    engine->bind_index_buffer(cube_mesh.index_buffer);

    engine->draw_indexed(cube_mesh.num_indices);
}
