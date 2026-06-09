#pragma once
#include <utility>
#include <stdexcept>
#include <unordered_map>
#include <vulkan/vulkan.h>

#include "fence.h"
#include "pipeline.h"
#include "image/image_transition_state.h"

class CommandPool;
class VideoBuffer;

class CommandBuffer {
public:
    struct SubmitDesc {
        VkQueue queue = VK_NULL_HANDLE;

        Fence* fence = nullptr;

        uint32_t wait_semaphore_count = 0;
        const VkSemaphore* wait_semaphores = nullptr;
        const VkPipelineStageFlags* wait_dst_stage_mask = nullptr;

        uint32_t signal_semaphore_count = 0;
        const VkSemaphore* signal_semaphores = nullptr;
    };

    VkDevice* device = nullptr;
    CommandPool* command_pool = nullptr;
    VkCommandBuffer buffer = VK_NULL_HANDLE;

    std::unordered_map<ImageResource*, ImageTransitionState> pending_image_states;

    CommandBuffer() = default;
    CommandBuffer(CommandPool* command_pool);
    
    ~CommandBuffer();
    void destroy();

    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;

    CommandBuffer(CommandBuffer&& other) noexcept;
    CommandBuffer& operator=(CommandBuffer&& other) noexcept;

    void begin(VkCommandBufferUsageFlags usage_flags = 0);
    void end();

    void bind_pipeline(Pipeline& pipeline);
    void dispatch(uint32_t x_groups, uint32_t y_groups, uint32_t z_groups);

    void buffer_barrier(
        VideoBuffer& video_buffer,
        VkPipelineStageFlags src_stage,
        VkAccessFlags src_access,
        VkPipelineStageFlags dst_stage,
        VkAccessFlags dst_access
    );

    void fill_buffer(VideoBuffer& video_buffer, uint32_t data);
    void submit(const SubmitDesc& desc);
    void submit(VkQueue queue, Fence* fence = nullptr);
    void submit(Fence* fence = nullptr);
    
    void submit_and_wait(VkQueue queue, Fence* fence = nullptr);
    void submit_and_wait(Fence* fence = nullptr);
};
