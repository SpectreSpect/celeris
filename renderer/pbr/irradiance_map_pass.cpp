#include "irradiance_map_pass.h"

#include "../../vulkan_self/vulkan_engine.h"
#include "../compute_pass_manager.h"
#include "../../vulkan_self/image/cubemap.h"
#include "../../math_utils.h"

IrradiancePass::IrradiancePass(
    VulkanEngine& engine,
    ComputePassManager& compute_pass_manager)
    :   m_engine(engine),
        uniform_buffer(VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(IrradianceMapGeneratorUniform))),
        m_irradiance_pass(compute_pass_manager.descriptor_pool(), compute_pass_manager.irradiance_map_cp),
        m_compute_command_buffer(engine.device(), engine.compute_command_pool()),
        m_compute_fence(engine.device()) {}

Cubemap IrradiancePass::generate(Cubemap& environment_map, uint32_t face_size) {
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

    Cubemap irradiance_map(
        m_engine.physical_device(),
        m_engine.device(),
        {face_size, face_size},
        VK_FORMAT_R32G32B32A32_SFLOAT,
        1
    );

    IrradianceMapGeneratorUniform uniform_data{};
    uniform_data.image_width = face_size;
    uniform_data.image_height = face_size;
    uniform_data.num_layers = Cubemap::face_count;

    uniform_buffer.upload(
        &uniform_data,
        sizeof(IrradianceMapGeneratorUniform)
    );

    m_irradiance_pass.set_uniform_buffer(0, uniform_buffer);
    m_irradiance_pass.set_cubemap(1, environment_map);
    m_irradiance_pass.set_storage_cubemap(2, irradiance_map);

    uint32_t x_groups = math_utils::div_up_u32(face_size, 16);
    uint32_t y_groups = math_utils::div_up_u32(face_size, 16);
    uint32_t z_groups = uniform_data.num_layers;

    {
        auto compute_scope = m_compute_command_buffer.begin_scope();

        irradiance_map.transition_layout(
            m_compute_command_buffer,
            VK_IMAGE_LAYOUT_GENERAL,

            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,

            0,
            VK_ACCESS_SHADER_WRITE_BIT
        );

        m_irradiance_pass.bind(m_compute_command_buffer);
        m_compute_command_buffer.dispatch(x_groups, y_groups, z_groups);

        irradiance_map.transition_layout(
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

    return irradiance_map;
}
