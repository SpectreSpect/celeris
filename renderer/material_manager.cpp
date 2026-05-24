#include "material_manager.h"

#include "../vulkan_self/vulkan_engine.h"
#include "../renderer/shader_manager.h"
#include "../renderer/resources/frame_resources.h"
#include "../vulkan_self/material/material_pass_builder.h"
#include "../vulkan_self/vulkan_shader_module.h"
#include "../vulkan_self/formats.h"
#include "../renderer/transform_push_constants.h"

MaterialManager::MaterialManager(VulkanEngine& engine, ShaderManager& shader_manager, FrameResources& frame_resources)
    :   blin_phong_mp(create_blin_phong_pass(engine, frame_resources, shader_manager.blinn_phong_vs, shader_manager.blinn_phong_fs)),
        unlit_mp(create_unlit_pass(engine, frame_resources, shader_manager.unlit_vs, shader_manager.unlit_fs)),
        m_pool(engine.device(), m_pool_builder) {}

MaterialPass MaterialManager::create_pass(VulkanEngine& engine, MaterialPassBuilder& builder, 
                                          const VulkanShaderModule& vs, const VulkanShaderModule& fs) {
    builder.add_vertex_shader(vs);
    builder.add_fragment_shader(fs);
    
    m_pool_builder.add_layout(builder.material_dsl_builder(), m_max_material_instances);
    
    return MaterialPass(engine, builder);
}

DescriptorPool& MaterialManager::descriptor_pool() noexcept {
    return m_pool;
}

MaterialPass MaterialManager::create_blin_phong_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                                     const VulkanShaderModule& vs, const VulkanShaderModule& fs) {
    LOG_METHOD();

    struct BlinPhongVertex {
        glm::vec4 position;
        glm::vec4 normal;
        glm::vec2 uv;
    };

    MaterialPassBuilder builder;

    builder.add_uniform_buffer(0, ShaderStages::fragment);
    builder.add_combined_image_sampler(1, ShaderStages::fragment);
    builder.add_uniform_buffer(2, ShaderStages::fragment);
    builder.add_storage_buffer(3, ShaderStages::fragment);

    builder.add_push_constants(sizeof(TransformPushConstants), 0);
    builder.add_descriptor_set_layout(frame_resources.descriptor_layout());

    builder.add_vertex_binding(0, sizeof(BlinPhongVertex));
    builder.add_vertex_attribute(0, 0, Formats::vec4, offsetof(BlinPhongVertex, position));
    builder.add_vertex_attribute(1, 0, Formats::vec4, offsetof(BlinPhongVertex, normal));
    builder.add_vertex_attribute(2, 0, Formats::vec2, offsetof(BlinPhongVertex, uv));

    return create_pass(engine, builder, vs, fs);
}


MaterialPass MaterialManager::create_unlit_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                                const VulkanShaderModule& vs, const VulkanShaderModule& fs) {
    LOG_METHOD();

    struct UnlitVertex {
        glm::vec4 position;
        glm::vec4 normal;
        glm::vec2 uv;
    };

    MaterialPassBuilder builder;

    builder.add_uniform_buffer(0, ShaderStages::fragment);
    builder.add_combined_image_sampler(1, ShaderStages::fragment);

    builder.add_push_constants(sizeof(TransformPushConstants), 0);
    builder.add_descriptor_set_layout(frame_resources.descriptor_layout());

    builder.add_vertex_binding(0, sizeof(UnlitVertex));
    builder.add_vertex_attribute(0, 0, Formats::vec4, offsetof(UnlitVertex, position));
    builder.add_vertex_attribute(1, 0, Formats::vec4, offsetof(UnlitVertex, normal));
    builder.add_vertex_attribute(2, 0, Formats::vec2, offsetof(UnlitVertex, uv));

    return create_pass(engine, builder, vs, fs);
}
