#include "frame_resources.h"

FrameResources::FrameResources(VulkanPhysicalDevice& physical_device, VulkanDevice& device, uint32_t num_frames_in_flight) 
    :   descriptor_layout_builder(create_dsl_builder()),
        m_descriptor_layout(device, descriptor_layout_builder),
        descriptor_pool(create_descriptor_pool(device, num_frames_in_flight)) {

    // DescriptorSetLayoutBuilder dsl_builder;
    // dsl_builder.add_uniform_buffer(0, ShaderStages::vertex_fragment); // camera uniform

    // DescriptorSetLayout dsl(device, dsl_builder);

    // DescriptorPoolBuilder pool_builder;
    // pool_builder.add_layout(dsl_builder);
    // DescriptorPool pool(device, pool_builder);

    descriptor_sets = descriptor_pool.allocate_sets(m_descriptor_layout, num_frames_in_flight);

    for (int i = 0; i < num_frames_in_flight; i++) {
        camera_uniforms.emplace_back(VulkanBuffer::create_host_visible_uniform_buffer(physical_device, device, sizeof(CameraUniform)));
        descriptor_sets[i].write_uniform_buffer(0, camera_uniforms[i]);
    }
}

DescriptorSetLayout& FrameResources::descriptor_layout() noexcept {
    return m_descriptor_layout;
}

void FrameResources::update_camera(uint32_t frame_id, const Camera& camera) {
    static CameraUniform ubo;

    float aspect = 1280.0f / 720.0f;

    ubo.proj = camera.get_projection_matrix(aspect);
    ubo.view = camera.get_view_matrix();
    ubo.view_pos = glm::vec4(camera.position, 1.0f);
    
    camera_uniforms[frame_id].upload(&ubo, sizeof(CameraUniform));
}

void FrameResources::bind(uint32_t frame_id, VulkanCommandBuffer& command_buffer, Pipeline& pipeline, uint32_t set_binding) {
    descriptor_sets[frame_id].bind(command_buffer, pipeline, set_binding);
}

DescriptorSetLayoutBuilder FrameResources::create_dsl_builder() {
    DescriptorSetLayoutBuilder dsl_builder;
    dsl_builder.add_uniform_buffer(0, ShaderStages::vertex_fragment); // camera uniform

    return dsl_builder;
}

DescriptorPool FrameResources::create_descriptor_pool(VulkanDevice& device, uint32_t num_frames_in_flight) {
    DescriptorPoolBuilder pool_builder;
    pool_builder.add_layout(descriptor_layout_builder, num_frames_in_flight);

    return DescriptorPool(device, pool_builder);
}