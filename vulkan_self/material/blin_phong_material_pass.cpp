// #include "blin_phong_material_pass.h"

// BlinPhongMaterialPass::BlinPhongMaterialPass(VulkanEngine& engine, DescriptorSetLayout& frame_resources_descriptor_layout, VulkanShaderModule& vertex_shader, VulkanShaderModule& fragment_shader)
//     :   MaterialPass(engine.device(), 
//                      create_dls_builder(), 
//                      create_pipeline_layout_builder(engine.device(), frame_resources_descriptor_layout), 
//                      create_pipeline_builder(engine, vertex_shader, fragment_shader)) {};

// DescriptorSetLayoutBuilder BlinPhongMaterialPass::create_dls_builder() {
//     DescriptorSetLayoutBuilder builder;
//     builder.add_uniform_buffer(0, ShaderStages::fragment);

//     return builder;
// }

// PipelineLayoutBuilder BlinPhongMaterialPass::create_pipeline_layout_builder(VulkanDevice& device, DescriptorSetLayout& frame_resources_descriptor_layout) {
//     PipelineLayoutBuilder pipeline_layout_builder = VulkanPipelineLayout::create_builder();
//     pipeline_layout_builder.set_device(device);
//     pipeline_layout_builder.add_push_constants<TransformPushConstants>();
//     // pipeline_layout_builder.add_descriptor_set_layout(dsl);
//     pipeline_layout_builder.add_descriptor_set_layout(m_descriptor_set_layout);
//     pipeline_layout_builder.add_descriptor_set_layout(frame_resources_descriptor_layout);

//     return pipeline_layout_builder;
// }

// GraphicsPipelineBuilder BlinPhongMaterialPass::create_pipeline_builder(VulkanEngine& engine, VulkanShaderModule& vertex_shader, VulkanShaderModule& fragment_shader) {
//     GraphicsPipelineBuilder pipeline_builder = GraphicsPipeline::create_builder();
//     pipeline_builder.set_graphic_objects(engine.device(), m_pipeline_layout, engine.swapchain_resources().render_pass);

//     struct Vertex {
//         glm::vec4 position;
//     };

//     VertexLayoutBuilder vertex_layout;
//     vertex_layout.add_binding(0, sizeof(Vertex));
//     vertex_layout.add_attribute(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, position));

//     pipeline_builder.set_vertex_layout(vertex_layout);
//     pipeline_builder.add_vert_shader_stage(vertex_shader);
//     pipeline_builder.add_frag_shader_stage(fragment_shader);

//     return pipeline_builder;
// }