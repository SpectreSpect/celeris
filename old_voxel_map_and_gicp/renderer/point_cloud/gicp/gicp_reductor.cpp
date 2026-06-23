#include "gicp_reductor.h"
#include "../../compute_pass_manager.h"
#include "../../../vulkan_self/vulkan_engine.h"
#include "../../../math_utils.h"

GICPReductor::GICPReductor(VulkanEngine& engine, ComputePassManager& compute_pass_manager) 
    :   engine(engine),
        gicp_reduce_pass(compute_pass_manager.descriptor_pool(), compute_pass_manager.gicp_reduce_cp),
        uniform_buffer(VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(GICPReductorUniform))),
        compute_command_buffer(engine.device(), engine.compute_command_pool()),
        compute_fence(engine.device()) {}

uint32_t GICPReductor::reduce_step(VulkanBuffer& partial_src, VulkanBuffer& partial_dst, const uint32_t input_count) {
    GICPReductorUniform uniform_data{};
    uniform_data.input_count = input_count;
    uniform_buffer.upload(&uniform_data, sizeof(GICPReductorUniform));

    gicp_reduce_pass.set_uniform_buffer(0, uniform_buffer);
    gicp_reduce_pass.set_storage_buffer(1, partial_src);
    gicp_reduce_pass.set_storage_buffer(2, partial_dst);

    uint32_t x_groups = math_utils::div_up_u32(input_count, 64);

    
    {
        auto compute_scope = compute_command_buffer.begin_scope();
        
        gicp_reduce_pass.bind(compute_command_buffer);

        compute_command_buffer.dispatch(x_groups, 1, 1);
    }

    compute_fence.reset();
    engine.compute_submit(compute_command_buffer, &compute_fence);
    compute_fence.wait();

    return x_groups;
}

GICPReductor::GICPPartial GICPReductor::reduce(VulkanBuffer& partial_src, VulkanBuffer& partial_dst, uint32_t input_count) {
    VulkanBuffer* src = &partial_src;
    VulkanBuffer* dst = &partial_dst;

    while (input_count > 1) {
        input_count = reduce_step(*src, *dst, input_count);
        std::swap(src, dst);
    }

    GICPPartial output{};
    src->read(&output, sizeof(GICPPartial), 0);

    return output;
}