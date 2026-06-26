#include "frame_resources.h"

#include "../lighting_system/lighting_system.h"

FrameResources::FrameResources(VulkanEngine& engine, LightingSystem& lighting_system, uint32_t num_frames_in_flight) 
    :   descriptor_layout_builder(create_dsl_builder()),
        m_descriptor_layout(engine.device(), descriptor_layout_builder),
        descriptor_pool(create_descriptor_pool(engine.device(), num_frames_in_flight)) {

    // DescriptorSetLayoutBuilder dsl_builder;
    // dsl_builder.add_uniform_buffer(0, ShaderStages::vertex_fragment); // camera uniform

    // DescriptorSetLayout dsl(device, dsl_builder);

    // DescriptorPoolBuilder pool_builder;
    // pool_builder.add_layout(dsl_builder);
    // DescriptorPool pool(device, pool_builder);


    // m_light_source_ssbo(VulkanBuffer::create_host_visible_storage_buffer(engine, sizeof(LightSource) * m_max_num_light_sources)),
    // m_lights_in_clusters_ssbo(VulkanBuffer::create_host_visible_storage_buffer(engine, sizeof(unsigned int) * m_lights_in_clusters_size)),
    // m_num_lights_in_clusters_ssbo(VulkanBuffer::create_host_visible_transfer_dst_storage_buffer(engine, sizeof(unsigned int) * m_total_clusters_count)),
    // m_cluster_aabbs_ssbo(VulkanBuffer::create_host_visible_storage_buffer(engine, sizeof(AABB) * m_total_clusters_count)),
    // m_uniform_buffer(VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(LightingSystemUniform))),


    // sizeof(LightSource) * m_max_num_light_sources
    // sizeof(unsigned int) * m_lights_in_clusters_size
    // sizeof(unsigned int) * m_total_clusters_count
    // sizeof(AABB) * m_total_clusters_count
    // sizeof(LightingSystemUniform)


    for (int i = 0; i < num_frames_in_flight; i++) {
        m_frame_data.emplace_back(
            engine,
            descriptor_pool,
            m_descriptor_layout,
            sizeof(CameraUniform),
            sizeof(LightSource) * lighting_system.max_num_light_sources(),
            sizeof(unsigned int) * lighting_system.lights_in_clusters_size(),
            sizeof(unsigned int) * lighting_system.total_clusters_count(),
            sizeof(AABB) * lighting_system.total_clusters_count(),
            sizeof(LightingSystem::LightingBuildUniform),
            sizeof(LightingSystem::ClusteredLightingUniform)
        );
    }

    // descriptor_sets = descriptor_pool.allocate_sets(m_descriptor_layout, num_frames_in_flight);

    for (int i = 0; i < num_frames_in_flight; i++) {
        // camera_uniforms.emplace_back(VulkanBuffer::create_host_visible_uniform_buffer(engine.physical_device(), engine.device(), sizeof(CameraUniform)));
        
        FrameData& frame_data = m_frame_data[i];
        DescriptorSet& frame_ds = frame_data.descriptor_set;


        frame_ds.write_uniform_buffer(0, frame_data.camera_uniform);

        frame_ds.write_uniform_buffer(3, frame_data.clustered_lighting_uniform);
        // frame_ds.write_storage_buffer(4, frame_data.cluster_aabbs_ssbo);
        frame_ds.write_storage_buffer(5, frame_data.light_source_ssbo);
        frame_ds.write_storage_buffer(6, frame_data.num_lights_in_clusters_ssbo);
        frame_ds.write_storage_buffer(7, frame_data.lights_in_clusters_ssbo);





        // dsl_builder.add_uniform_buffer(0, ShaderStages::vertex_fragment); // camera uniform

        // dsl_builder.add_uniform_buffer(3, ShaderStages::vertex_fragment); // lighting_system_uniform_buffer
        // dsl_builder.add_storage_buffer(4, ShaderStages::vertex_fragment); // cluster_aabbs_ssbo
        // dsl_builder.add_storage_buffer(5, ShaderStages::vertex_fragment); // light_source_ssbo
        // dsl_builder.add_storage_buffer(6, ShaderStages::vertex_fragment); // num_lights_in_clusters_ssbo
        // dsl_builder.add_storage_buffer(7, ShaderStages::vertex_fragment); // lights_in_clusters_ssbo
    }
}

DescriptorSetLayout& FrameResources::descriptor_layout() noexcept {
    return m_descriptor_layout;
}

void FrameResources::update_camera(uint32_t frame_id, const Window& window, const Camera& camera) {
    LOG_METHOD();

    logger.check(frame_id < m_frame_data.size(), "Frame index was out of bounds");

    static CameraUniform ubo;

    float aspect = float(window.width()) / float(window.height());

    ubo.proj = camera.get_projection_matrix(aspect);
    ubo.view = camera.get_view_matrix();
    ubo.view_pos = glm::vec4(camera.position, 1.0f);
    ubo.viewport = {window.width(), window.height()};

    m_frame_data[frame_id].camera_uniform.upload(&ubo, sizeof(CameraUniform));
}

void FrameResources::bind(uint32_t frame_id, VulkanCommandBuffer& command_buffer, Pipeline& pipeline, uint32_t set_binding) {
    m_frame_data[frame_id].descriptor_set.bind(command_buffer, pipeline, set_binding);
}

FrameData& FrameResources::frame_data(uint32_t frame_id) {
    LOG_METHOD();

    logger.check(frame_id < m_frame_data.size(), "Frame index was out of bounds");

    return m_frame_data[frame_id];
}

DescriptorSetLayoutBuilder FrameResources::create_dsl_builder() {
    DescriptorSetLayoutBuilder dsl_builder;

    dsl_builder.add_uniform_buffer(0, ShaderStages::vertex_fragment); // camera uniform

    dsl_builder.add_uniform_buffer(3, ShaderStages::vertex_fragment); // lighting_system_uniform_buffer
    // dsl_builder.add_storage_buffer(4, ShaderStages::vertex_fragment); // cluster_aabbs_ssbo
    dsl_builder.add_storage_buffer(5, ShaderStages::vertex_fragment); // light_source_ssbo
    dsl_builder.add_storage_buffer(6, ShaderStages::vertex_fragment); // num_lights_in_clusters_ssbo
    dsl_builder.add_storage_buffer(7, ShaderStages::vertex_fragment); // lights_in_clusters_ssbo

    return dsl_builder;
}

DescriptorPool FrameResources::create_descriptor_pool(VulkanDevice& device, uint32_t num_frames_in_flight) {
    DescriptorPoolBuilder pool_builder;
    pool_builder.add_layout(descriptor_layout_builder, num_frames_in_flight);

    return DescriptorPool(device, pool_builder);
}