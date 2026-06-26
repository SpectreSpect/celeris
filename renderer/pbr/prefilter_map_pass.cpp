#include "prefilter_map_pass.h"

#include <algorithm>

#include "../../vulkan_self/vulkan_engine.h"
#include "../../managers/compute_pass_manager.h"
#include "../../vulkan_self/image/cubemap.h"
#include "../../vulkan_self/image/cubemap_array.h"
#include "../../vulkan_self/image/vulkan_image_view.h"
#include "../../math_utils.h"

PrefilterPass::PrefilterPass(
    VulkanEngine& engine,
    ComputePassManager& compute_pass_manager)
    :   m_engine(engine),
        uniform_buffer(VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(PrefilterMapGeneratorUniform))),
        m_prefilter_pass(compute_pass_manager.prefilter_map_cp, compute_pass_manager.descriptor_pool()),
        m_compute_command_buffer(engine.device(), engine.compute_command_pool()),
        m_compute_fence(engine.device()) {}

Cubemap PrefilterPass::generate(Cubemap& environment_map, uint32_t face_size) {
    LOG_METHOD();

    logger.check(face_size != 0, "Face size must be greater than 0");
    logger.check(environment_map.image().handle() != VK_NULL_HANDLE, "Environment map image is not initialized");
    logger.check(environment_map.view().handle() != VK_NULL_HANDLE, "Environment map image view is not initialized");
    logger.check(environment_map.sampler().handle() != VK_NULL_HANDLE, "Environment map sampler is not initialized");
    logger.check(environment_map.array_layers() == Cubemap::face_count, "Environment map must have 6 faces");
    logger.check(
        environment_map.layout() == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        "Environment map must be in VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL"
    );

    uint32_t mip_levels = Cubemap::calculate_mip_levels({face_size, face_size});

    Cubemap prefilter_map(
        m_engine.physical_device(),
        m_engine.device(),
        {face_size, face_size},
        VK_FORMAT_R32G32B32A32_SFLOAT,
        mip_levels
    );

    m_prefilter_pass.set_uniform_buffer(0, uniform_buffer);
    m_prefilter_pass.set_cubemap(1, environment_map);

    // Transition the whole output cubemap once. Every mip will be written as a storage image.
    {
        auto compute_scope = m_compute_command_buffer.begin_scope();

        prefilter_map.transition_layout(
            m_compute_command_buffer,
            VK_IMAGE_LAYOUT_GENERAL,

            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,

            0,
            VK_ACCESS_SHADER_WRITE_BIT
        );
    }

    m_compute_fence.reset();
    m_engine.compute_submit(m_compute_command_buffer, &m_compute_fence);
    m_compute_fence.wait();

    // One submit per mip is intentional.
    // The storage descriptor points at a different one-mip image view each iteration.
    for (uint32_t mip = 0; mip < mip_levels; ++mip) {
        uint32_t mip_face_size = std::max(1u, face_size >> mip);

        float roughness = mip_levels > 1
            ? static_cast<float>(mip) / static_cast<float>(mip_levels - 1)
            : 0.0f;

        PrefilterMapGeneratorUniform uniform_data{};
        uniform_data.face_size = mip_face_size;
        uniform_data.num_layers = Cubemap::face_count;
        uniform_data.roughness = roughness;
        uniform_data.environment_resolution = static_cast<float>(environment_map.extent().width);

        uniform_buffer.upload(&uniform_data, sizeof(PrefilterMapGeneratorUniform));

        VulkanImageView mip_view(
            m_engine.device(),
            prefilter_map.image(),
            VK_IMAGE_ASPECT_COLOR_BIT,
            mip,
            1,
            0,
            Cubemap::face_count,
            VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
        );

        m_prefilter_pass.set_storage_image_view(2, mip_view);

        uint32_t x_groups = math_utils::div_up_u32(mip_face_size, 16);
        uint32_t y_groups = math_utils::div_up_u32(mip_face_size, 16);
        uint32_t z_groups = Cubemap::face_count;

        {
            auto compute_scope = m_compute_command_buffer.begin_scope();

            m_prefilter_pass.bind(m_compute_command_buffer);
            m_compute_command_buffer.dispatch(x_groups, y_groups, z_groups);
        }

        m_compute_fence.reset();
        m_engine.compute_submit(m_compute_command_buffer, &m_compute_fence);
        m_compute_fence.wait();
    }

    // The generated cubemap will be sampled later by PBR shaders.
    {
        auto compute_scope = m_compute_command_buffer.begin_scope();

        prefilter_map.transition_layout(
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

    return prefilter_map;
}

void PrefilterPass::generate_into(Cubemap& environment_map, CubemapArray& prefilter_maps, uint32_t cubemap_id) {
    LOG_METHOD();

    logger.check(prefilter_maps.extent().width != 0, "Prefilter cubemap array width is zero");
    logger.check(prefilter_maps.extent().height != 0, "Prefilter cubemap array height is zero");
    logger.check(prefilter_maps.extent().width == prefilter_maps.extent().height, "Prefilter cubemap array faces must be square");
    logger.check(prefilter_maps.format() == VK_FORMAT_R32G32B32A32_SFLOAT, "Prefilter cubemap array must use VK_FORMAT_R32G32B32A32_SFLOAT");
    logger.check(cubemap_id < prefilter_maps.cubemap_count(), "Prefilter cubemap id is out of range");

    logger.check(environment_map.image().handle() != VK_NULL_HANDLE, "Environment map image is not initialized");
    logger.check(environment_map.view().handle() != VK_NULL_HANDLE, "Environment map image view is not initialized");
    logger.check(environment_map.sampler().handle() != VK_NULL_HANDLE, "Environment map sampler is not initialized");
    logger.check(environment_map.array_layers() == Cubemap::face_count, "Environment map must have 6 faces");
    logger.check(
        environment_map.layout() == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        "Environment map must be in VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL"
    );

    uint32_t face_size = prefilter_maps.extent().width;
    uint32_t mip_levels = prefilter_maps.mip_levels();

    m_prefilter_pass.set_uniform_buffer(0, uniform_buffer);
    m_prefilter_pass.set_cubemap(1, environment_map);

    {
        auto compute_scope = m_compute_command_buffer.begin_scope();

        if (prefilter_maps.layout() == VK_IMAGE_LAYOUT_UNDEFINED) {
            prefilter_maps.transition_layout(
                m_compute_command_buffer,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

                0,
                VK_ACCESS_SHADER_READ_BIT
            );
        }

        prefilter_maps.transition_layout(
            m_compute_command_buffer,
            VK_IMAGE_LAYOUT_GENERAL,

            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,

            VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,

            0,
            mip_levels,
            prefilter_maps.base_layer(cubemap_id),
            CubemapArray::face_count
        );
    }

    m_compute_fence.reset();
    m_engine.compute_submit(m_compute_command_buffer, &m_compute_fence);
    m_compute_fence.wait();

    for (uint32_t mip = 0; mip < mip_levels; ++mip) {
        uint32_t mip_face_size = std::max(1u, face_size >> mip);

        float roughness = mip_levels > 1
            ? static_cast<float>(mip) / static_cast<float>(mip_levels - 1)
            : 0.0f;

        PrefilterMapGeneratorUniform uniform_data{};
        uniform_data.face_size = mip_face_size;
        uniform_data.num_layers = Cubemap::face_count;
        uniform_data.roughness = roughness;
        uniform_data.environment_resolution = static_cast<float>(environment_map.extent().width);

        uniform_buffer.upload(&uniform_data, sizeof(PrefilterMapGeneratorUniform));

        VulkanImageView mip_view(
            m_engine.device(),
            prefilter_maps.image(),
            VK_IMAGE_ASPECT_COLOR_BIT,
            mip,
            1,
            prefilter_maps.base_layer(cubemap_id),
            CubemapArray::face_count,
            VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
        );

        m_prefilter_pass.set_storage_image_view(2, mip_view);

        uint32_t x_groups = math_utils::div_up_u32(mip_face_size, 16);
        uint32_t y_groups = math_utils::div_up_u32(mip_face_size, 16);
        uint32_t z_groups = Cubemap::face_count;

        {
            auto compute_scope = m_compute_command_buffer.begin_scope();

            m_prefilter_pass.bind(m_compute_command_buffer);
            m_compute_command_buffer.dispatch(x_groups, y_groups, z_groups);
        }

        m_compute_fence.reset();
        m_engine.compute_submit(m_compute_command_buffer, &m_compute_fence);
        m_compute_fence.wait();
    }

    {
        auto compute_scope = m_compute_command_buffer.begin_scope();

        prefilter_maps.transition_layout(
            m_compute_command_buffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,

            0,
            mip_levels,
            prefilter_maps.base_layer(cubemap_id),
            CubemapArray::face_count
        );
    }

    m_compute_fence.reset();
    m_engine.compute_submit(m_compute_command_buffer, &m_compute_fence);
    m_compute_fence.wait();
}
