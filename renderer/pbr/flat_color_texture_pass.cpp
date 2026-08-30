#include "flat_color_texture_pass.h"

#include "../../managers/compute_pass_manager.h"
#include "../../math_utils.h"
#include "../../vulkan_self/image/vulkan_texture_2d.h"
#include "../../vulkan_self/vulkan_engine.h"

FlatColorTexturePass::FlatColorTexturePass(
    VulkanEngine& engine,
    ComputePassManager& compute_pass_manager)
    : m_engine(engine),
      m_uniform_buffer(VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(UniformData))),
      m_pass(compute_pass_manager.flat_color_texture_cp, compute_pass_manager.descriptor_pool()),
      m_compute_command_buffer(engine.device(), engine.compute_command_pool()),
      m_compute_fence(engine.device()) {}

VulkanTexture2D FlatColorTexturePass::generate(
    uint32_t width,
    uint32_t height,
    const glm::vec4& color)
{
    LOG_METHOD();

    logger().check(width != 0, "Width must be greater than 0");
    logger().check(height != 0, "Height must be greater than 0");

    VulkanTexture2DDesc desc = VulkanTexture2D::default_desc({width, height});
    desc.format = VK_FORMAT_R8G8B8A8_UNORM;
    desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    desc.mip_levels = 1;
    desc.sampler_address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    desc.sampler_address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    desc.sampler_address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    desc.sampler_mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    desc.sampler_min_lod = 0.0f;
    desc.sampler_max_lod = 0.0f;

    VulkanTexture2D texture(m_engine.physical_device(), m_engine.device(), desc);

    const UniformData uniform_data{
        .image_width = width,
        .image_height = height,
        .padding = {0, 0},
        .color = color
    };
    m_uniform_buffer.upload(&uniform_data, sizeof(uniform_data));
    m_pass.set_uniform_buffer(0, m_uniform_buffer);

    const uint32_t x_groups = math_utils::div_up_u32(width, 16);
    const uint32_t y_groups = math_utils::div_up_u32(height, 16);

    {
        auto compute_scope = m_compute_command_buffer.begin_scope();

        texture.transition_layout(
            m_compute_command_buffer,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            VK_ACCESS_SHADER_WRITE_BIT
        );

        m_pass.set_storage_texture(1, texture);
        m_pass.bind(m_compute_command_buffer);
        m_compute_command_buffer.dispatch(x_groups, y_groups, 1);

        texture.transition_layout(
            m_compute_command_buffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );
    }

    m_compute_fence.reset();
    m_engine.compute_submit(m_compute_command_buffer, &m_compute_fence);
    m_compute_fence.wait();

    return texture;
}
