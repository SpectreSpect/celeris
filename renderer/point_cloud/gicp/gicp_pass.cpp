#include "gicp_pass.h"

#include "../../compute_pass_manager.h"

GICPPass::GICPPass(VulkanEngine& engine, ComputePassManager& compute_pass_manager) 
    :   gicp_step_pass(compute_pass_manager.descriptor_pool(), compute_pass_manager.gicp_cp),
        uniform_buffer(VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(GICPPassUniform))),
        output_buffer(VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(OutputBuffer))),
        partial_src(VulkanBuffer::create_storage_buffer(engine, sizeof(GICPPass::GICPPartial) * max_partial_count)),
        partial_dst(VulkanBuffer::create_storage_buffer(engine, sizeof(GICPPass::GICPPartial) * max_partial_count)){
    

    // partial_src.create(engine, sizeof(GICPReductor::GICPPartial) * max_partial_count);
    // partial_dst.create(engine, sizeof(GICPReductor::GICPPartial) * max_partial_count);
    // rejection_buffer.create(engine, sizeof(uint32_t) * max_partial_count);
    // // debug_buffer.create(engine, sizeof(uint32_t) * max_partial_count);
    // reductor = GICPReductor(engine);
}