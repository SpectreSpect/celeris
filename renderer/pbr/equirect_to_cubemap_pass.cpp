#include "equirect_to_cubemap_pass.h"

#include "../../vulkan_self/vulkan_engine.h"
#include "../../managers/compute_pass_manager.h"
#include "../../vulkan_self/image/vulkan_texture_2d.h"
#include "../../vulkan_self/image/cubemap.h"
#include "../../math_utils.h"

EquirectToCubemapPass::EquirectToCubemapPass(VulkanEngine& engine, ComputePassManager& compute_pass_manager)
    :   m_engine(engine),
        uniform_buffer(VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(EquirectToCubemapUniform))),
        m_equirect_to_cubemap_pass(compute_pass_manager.equirect_to_cubemap_cp, compute_pass_manager.descriptor_pool()),
        m_compute_command_buffer(engine.device(), engine.compute_command_pool()),
        m_compute_fence(engine.device()){
}

Cubemap EquirectToCubemapPass::generate(VulkanTexture2D& equirectangular_map, uint32_t face_size) {
    LOG_METHOD();

    logger.check(face_size != 0, "Face size must be greater than 0");

    uint32_t mip_levels = Cubemap::calculate_mip_levels({face_size, face_size});

    Cubemap cubemap(m_engine.physical_device(), m_engine.device(), {face_size, face_size}, VK_FORMAT_R32G32B32A32_SFLOAT, mip_levels);

    static EquirectToCubemapUniform uniform_data{};
    uniform_data.image_width = face_size;
    uniform_data.image_height = face_size;
    uniform_data.num_layers = 6;
    uniform_buffer.upload(&uniform_data, sizeof(EquirectToCubemapUniform));

    m_equirect_to_cubemap_pass.set_uniform_buffer(0, uniform_buffer);
    m_equirect_to_cubemap_pass.set_texture(1, equirectangular_map);
    m_equirect_to_cubemap_pass.set_storage_cubemap(2, cubemap);

    uint32_t x_groups = math_utils::div_up_u32(uniform_data.image_width, 256);
    uint32_t y_groups = uniform_data.image_height;
    uint32_t z_groups = uniform_data.num_layers;

    {
        auto compute_scope = m_compute_command_buffer.begin_scope();

        cubemap.transition_layout(
            m_compute_command_buffer,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            VK_ACCESS_SHADER_WRITE_BIT
        );
        
        m_equirect_to_cubemap_pass.bind(m_compute_command_buffer);

        m_compute_command_buffer.dispatch(x_groups, y_groups, z_groups);

        cubemap.generate_mipmaps(m_compute_command_buffer);
    }

    m_compute_fence.reset();
    m_engine.compute_submit(m_compute_command_buffer, &m_compute_fence);
    m_compute_fence.wait();

    return cubemap;
}

void EquirectToCubemapPass::generate_cubemap_mipmaps(
    VulkanCommandBuffer& command_buffer,
    Cubemap& cubemap)
{
    LOG_METHOD();

    logger.check(cubemap.image().handle() != VK_NULL_HANDLE, "Cubemap image is not initialized");
    logger.check(cubemap.mip_levels() != 0, "Cubemap mip levels count is zero");
    logger.check(cubemap.array_layers() == Cubemap::face_count, "Cubemap must have 6 faces");

    logger.check(
        cubemap.image().has_usage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
        "Cubemap image must have VK_IMAGE_USAGE_TRANSFER_SRC_BIT for mip generation"
    );

    logger.check(
        cubemap.image().has_usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT),
        "Cubemap image must have VK_IMAGE_USAGE_TRANSFER_DST_BIT for mip generation"
    );

    if (cubemap.mip_levels() == 1) {
        cubemap.image().memory_barrier(
            command_buffer,

            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,

            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT,

            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

            VK_IMAGE_ASPECT_COLOR_BIT,

            0,
            1,

            0,
            Cubemap::face_count
        );

        cubemap.set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        return;
    }

    int32_t mip_width = static_cast<int32_t>(cubemap.extent().width);
    int32_t mip_height = static_cast<int32_t>(cubemap.extent().height);

    for (uint32_t mip = 1; mip < cubemap.mip_levels(); mip++) {
        VkImageLayout previous_mip_old_layout =
            mip == 1
                ? VK_IMAGE_LAYOUT_GENERAL
                : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        VkPipelineStageFlags previous_mip_old_stage =
            mip == 1
                ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                : VK_PIPELINE_STAGE_TRANSFER_BIT;

        VkAccessFlags previous_mip_old_access =
            mip == 1
                ? VK_ACCESS_SHADER_WRITE_BIT
                : VK_ACCESS_TRANSFER_WRITE_BIT;

        // Previous mip becomes the blit source.
        cubemap.image().memory_barrier(
            command_buffer,

            previous_mip_old_stage,
            previous_mip_old_access,

            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,

            previous_mip_old_layout,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,

            VK_IMAGE_ASPECT_COLOR_BIT,

            mip - 1,
            1,

            0,
            Cubemap::face_count
        );

        // Current mip becomes the blit destination.
        cubemap.image().memory_barrier(
            command_buffer,

            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            0,

            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,

            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,

            VK_IMAGE_ASPECT_COLOR_BIT,

            mip,
            1,

            0,
            Cubemap::face_count
        );

        int32_t next_mip_width = std::max(mip_width / 2, 1);
        int32_t next_mip_height = std::max(mip_height / 2, 1);

        VkImageBlit blit{};
        blit.srcOffsets[0] = VkOffset3D{0, 0, 0};
        blit.srcOffsets[1] = VkOffset3D{mip_width, mip_height, 1};

        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = mip - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = Cubemap::face_count;

        blit.dstOffsets[0] = VkOffset3D{0, 0, 0};
        blit.dstOffsets[1] = VkOffset3D{next_mip_width, next_mip_height, 1};

        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = mip;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = Cubemap::face_count;

        vkCmdBlitImage(
            command_buffer.handle(),

            cubemap.image().handle(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,

            cubemap.image().handle(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,

            1,
            &blit,

            VK_FILTER_LINEAR
        );

        // Previous mip is finished. Make it shader-readable.
        cubemap.image().memory_barrier(
            command_buffer,

            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,

            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT,

            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

            VK_IMAGE_ASPECT_COLOR_BIT,

            mip - 1,
            1,

            0,
            Cubemap::face_count
        );

        mip_width = next_mip_width;
        mip_height = next_mip_height;
    }

    // Last mip was only used as TRANSFER_DST, so transition it too.
    cubemap.image().memory_barrier(
        command_buffer,

        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,

        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT,

        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

        VK_IMAGE_ASPECT_COLOR_BIT,

        cubemap.mip_levels() - 1,
        1,

        0,
        Cubemap::face_count
    );

    cubemap.set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
