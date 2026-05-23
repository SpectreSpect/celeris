#include "material_pass.h"

MaterialPass::MaterialPass(VulkanDevice& device, VulkanRenderPass& render_pass, MaterialPassBuilder& builder)
    :   m_descriptor_set_layout(std::move(builder.create_descriptor_set_layout(device))),
        m_pipeline_layout(builder.create_pipeline_layout(device, m_descriptor_set_layout)),
        m_pipeline(builder.create_graphics_pipeline(device, render_pass, m_pipeline_layout)) {}


MaterialPass::MaterialPass(VulkanEngine& engine, MaterialPassBuilder& builder)
    :   MaterialPass(engine.device(), engine.swapchain_resources().render_pass, builder){}