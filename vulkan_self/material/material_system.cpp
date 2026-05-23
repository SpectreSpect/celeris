#include "material_system.h"

#include "../descriptor_set/descriptor_pool_builder.h"
#include "blin_phong_material_layout.h"
#include "../vulkan_buffer.h"
#include "material_instance.h"
#include "material_pass_builder.h"

MaterialSystem::MaterialSystem(VulkanDevice& device) 
    :   m_descriptor_pool(build_descriptor_pool(device)){}

DescriptorPool MaterialSystem::build_descriptor_pool(VulkanDevice& device) {
    DescriptorPoolBuilder pool_builder;

    pool_builder.set_max_sets(256);
    pool_builder.add_descriptors(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256);
    pool_builder.add_descriptors(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256);
    pool_builder.add_descriptors(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256);

    return DescriptorPool(device, pool_builder);
}

MaterialPass MaterialSystem::create_blin_phong_pass(
    VulkanEngine& engine,
    const DescriptorSetLayout& frame_resources_descriptor_layout,
    const VulkanShaderModule& vertex_shader,
    const VulkanShaderModule& fragment_shader)
{
    LOG_NAMED("MaterialSystem");

    struct BlinPhongVertex {
        glm::vec4 position;
        glm::vec4 normal;
        glm::vec2 uv;
    };

    MaterialPassBuilder builder;

    builder.add_uniform_buffer(0, ShaderStages::fragment);
    builder.add_combined_image_sampler(1, ShaderStages::fragment);

    builder.add_push_constants(sizeof(TransformPushConstants), 0);
    builder.add_descriptor_set_layout(frame_resources_descriptor_layout);

    builder.add_vertex_binding(0, sizeof(BlinPhongVertex));
    builder.add_vertex_attribute(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(BlinPhongVertex, position));
    builder.add_vertex_attribute(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(BlinPhongVertex, normal));
    builder.add_vertex_attribute(2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(BlinPhongVertex, uv));

    builder.add_vertex_shader(vertex_shader);
    builder.add_fragment_shader(fragment_shader);

    return MaterialPass(engine, builder);
}

MaterialPass MaterialSystem::create_unlit_pass(
    VulkanEngine& engine,
    const DescriptorSetLayout& frame_resources_descriptor_layout,
    const VulkanShaderModule& vertex_shader,
    const VulkanShaderModule& fragment_shader
) {
    LOG_NAMED("MaterialSystem");

    struct UnlitVertex {
        glm::vec4 position;
        glm::vec4 normal;
        glm::vec2 uv;
    };

    MaterialPassBuilder builder;

    builder.add_uniform_buffer(0, ShaderStages::fragment);
    builder.add_combined_image_sampler(1, ShaderStages::fragment);

    builder.add_push_constants(sizeof(TransformPushConstants), 0);
    builder.add_descriptor_set_layout(frame_resources_descriptor_layout);

    builder.add_vertex_binding(0, sizeof(UnlitVertex));
    builder.add_vertex_attribute(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(UnlitVertex, position));
    builder.add_vertex_attribute(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(UnlitVertex, normal));
    builder.add_vertex_attribute(2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(UnlitVertex, uv));

    builder.add_vertex_shader(vertex_shader);
    builder.add_fragment_shader(fragment_shader);

    return MaterialPass(engine, builder);
}

DescriptorPool& MaterialSystem::descriptor_pool() noexcept {
    return m_descriptor_pool;
}