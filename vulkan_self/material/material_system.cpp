#include "material_system.h"

#include "../descriptor_set/descriptor_pool_builder.h"
#include "blin_phong_material_layout.h"
#include "../vulkan_buffer.h"
#include "material_instance.h"

MaterialSystem::MaterialSystem(VulkanDevice& device) 
    :   m_blin_phong_layout(device),
        m_descriptor_pool(build_descriptor_pool(device)){}

DescriptorPool MaterialSystem::build_descriptor_pool(VulkanDevice& device) {
    DescriptorPoolBuilder pool_builder;

    pool_builder.add_layout(m_blin_phong_layout.descriptor_set_layout_builder(), 256);

    return DescriptorPool(device, pool_builder);
}

MaterialInstance MaterialSystem::create_blin_phong_material(VulkanBuffer& blin_phong_uniform, glm::vec4 albedo) {
    blin_phong_uniform.upload(&albedo, sizeof(glm::vec4));

    MaterialInstance instance(m_descriptor_pool, m_blin_phong_layout);
    instance.descriptor_set.write_uniform_buffer(0, blin_phong_uniform);

    
    return instance;
}

MaterialPass MaterialSystem::create_blin_phong_pass(
    VulkanEngine& engine,
    const DescriptorSetLayout& frame_resources_descriptor_layout,
    const VulkanShaderModule& vertex_shader,
    const VulkanShaderModule& fragment_shader
) {
    LOG_NAMED("MaterialSystem");

    /*
        set = 0 -> material data
        set = 1 -> frame/camera data
    */

    DescriptorSetLayoutBuilder material_dsl_builder;
    material_dsl_builder.add_uniform_buffer(0, ShaderStages::fragment);

    DescriptorSetLayout material_descriptor_set_layout(
        engine.device(),
        material_dsl_builder
    );

    PipelineLayoutBuilder pipeline_layout_builder =
        VulkanPipelineLayout::create_builder();

    pipeline_layout_builder.set_device(engine.device());

    pipeline_layout_builder.add_push_constants<TransformPushConstants>();

    pipeline_layout_builder.add_descriptor_set_layout(material_descriptor_set_layout);

    pipeline_layout_builder.add_descriptor_set_layout(frame_resources_descriptor_layout);

    VulkanPipelineLayout pipeline_layout(pipeline_layout_builder);

    GraphicsPipelineBuilder pipeline_builder = GraphicsPipeline::create_builder();

    pipeline_builder.set_graphic_objects(engine.device(), pipeline_layout, engine.swapchain_resources().render_pass);

    struct BlinPhongVertex {
        glm::vec4 position;
    };

    VertexLayoutBuilder vertex_layout;
    vertex_layout.add_binding(0, sizeof(BlinPhongVertex));

    vertex_layout.add_attribute(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(BlinPhongVertex, position));

    pipeline_builder.set_vertex_layout(vertex_layout);

    pipeline_builder.add_vert_shader_stage(vertex_shader);
    pipeline_builder.add_frag_shader_stage(fragment_shader);

    GraphicsPipeline pipeline(pipeline_builder);

    return MaterialPass(
        std::move(material_descriptor_set_layout),
        std::move(pipeline_layout),
        std::move(pipeline),
        0 // material_set_index
    );
}