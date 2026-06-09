#include "command_buffer.h"
#include "command_pool.h"
#include "video_buffer.h"
#include "vulkan_utils.h"

CommandBuffer::CommandBuffer(CommandPool* command_pool)
{
    if (command_pool == nullptr) {
        std::string message = "CommandBuffer::CommandBuffer: command_pool was nullptr.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (command_pool->device == nullptr) {
        std::string message = "CommandBuffer::CommandBuffer: command_pool->device was nullptr.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    this->command_pool = command_pool;
    this->device = command_pool->device;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = command_pool->pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    vulkan_utils::vk_check(
        vkAllocateCommandBuffers(*command_pool->device, &allocInfo, &buffer),
        "vkAllocateCommandBuffers"
    );
}

CommandBuffer::~CommandBuffer() {
    destroy();
}

void CommandBuffer::destroy() {
    if (buffer != VK_NULL_HANDLE && device == nullptr) {
        std::string message = 
            "CommandBuffer::destroy: Memory leak due to inability to destroy object "
            "- device is nullptr.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (buffer != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(*device, command_pool->pool, 1, &buffer);
        buffer = VK_NULL_HANDLE;
    }
}

CommandBuffer::CommandBuffer(CommandBuffer&& other) noexcept
    :   device(std::exchange(other.device, nullptr)),
        command_pool(std::exchange(other.command_pool, nullptr)),
        buffer(std::exchange(buffer, VK_NULL_HANDLE)),
        pending_image_states(std::move(pending_image_states))
{

}

CommandBuffer& CommandBuffer::operator=(CommandBuffer&& other) noexcept 
{
    if (this == &other)
        return *this;
    
    destroy();

    device = std::exchange(other.device, nullptr);
    command_pool = std::exchange(other.command_pool, nullptr);
    buffer = std::exchange(buffer, VK_NULL_HANDLE);
    pending_image_states = std::move(pending_image_states);
}

void CommandBuffer::begin(VkCommandBufferUsageFlags usage_flags) {
    if (!device)
        throw std::runtime_error("device was nullptr");

    vulkan_utils::vk_check(vkResetCommandBuffer(buffer, 0), "vkResetCommandBuffer");

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = usage_flags;

    vulkan_utils::vk_check(vkBeginCommandBuffer(buffer, &begin_info), "vkBeginCommandBuffer");
}

void CommandBuffer::end() {
    vulkan_utils::vk_check(vkEndCommandBuffer(buffer), "vkEndCommandBuffer");
}

void CommandBuffer::bind_pipeline(Pipeline& pipeline) {
    vkCmdBindPipeline(buffer, pipeline.get_bind_point(), pipeline.pipeline);
    vkCmdBindDescriptorSets(
        buffer,
        pipeline.get_bind_point(),
        pipeline.pipeline_layout,
        0,
        1,
        &pipeline.descriptor_set_bundle->descriptor_set.descriptor_set,
        0,
        nullptr
    );
}

void CommandBuffer::dispatch(uint32_t x_groups, uint32_t y_groups, uint32_t z_groups) {
    vkCmdDispatch(buffer, x_groups, y_groups, z_groups);
}

void CommandBuffer::fill_buffer(VideoBuffer& video_buffer, uint32_t data) {
    vkCmdFillBuffer(buffer, video_buffer.buffer, 0, video_buffer.size_bytes, data);
}

void CommandBuffer::buffer_barrier(
    VideoBuffer& video_buffer,
    VkPipelineStageFlags src_stage,
    VkAccessFlags src_access,
    VkPipelineStageFlags dst_stage,
    VkAccessFlags dst_access
) {
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = video_buffer.buffer;
    barrier.offset = 0;
    barrier.size = video_buffer.size_bytes;

    vkCmdPipelineBarrier(
        buffer,
        src_stage,
        dst_stage,
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr
    );
}

void CommandBuffer::submit(const SubmitDesc& desc) {
    if (device == nullptr) {
        std::string message = "CommandBuffer::submit: device was nullptr.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (desc.queue == VK_NULL_HANDLE) {
        std::string message = "CommandBuffer::submit: submit queue was VK_NULL_HANDLE.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (desc.fence) {
        desc.fence->reset();
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = desc.wait_semaphore_count;
    submitInfo.pWaitSemaphores = desc.wait_semaphores;
    submitInfo.pWaitDstStageMask = desc.wait_dst_stage_mask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &buffer;
    submitInfo.signalSemaphoreCount = desc.signal_semaphore_count;
    submitInfo.pSignalSemaphores = desc.signal_semaphores;

    vulkan_utils::vk_check(
        vkQueueSubmit(desc.queue, 1, &submitInfo, desc.fence ? desc.fence->fence : VK_NULL_HANDLE),
        "vkQueueSubmit"
    );

    for (auto it = pending_image_states.begin(); it != pending_image_states.end(); it++) {
        it->first->transition_state = std::move(it->second);
    }

    pending_image_states.clear();
}

void CommandBuffer::submit(VkQueue queue, Fence* fence) {
    SubmitDesc desc{};
    desc.queue = queue;
    desc.fence = fence;
    submit(desc);
}

void CommandBuffer::submit(Fence* fence) {
    if (command_pool == nullptr) {
        std::string message = "CommandBuffer::submit_and_wait: command_pool was nullptr.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (command_pool->queue == nullptr) {
        std::string message = "CommandBuffer::submit_and_wait: command_pool->queue was nullptr.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    submit(*command_pool->queue, fence);
}

void CommandBuffer::submit_and_wait(VkQueue queue, Fence* fence) {
    submit(queue, fence);

    if (fence != nullptr)
        fence->wait_for_fence();
}

void CommandBuffer::submit_and_wait(Fence* fence) {
    if (command_pool == nullptr) {
        std::string message = "CommandBuffer::submit_and_wait: command_pool was nullptr.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (command_pool->queue == nullptr) {
        std::string message = "CommandBuffer::submit_and_wait: command_pool->queue was nullptr.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (fence != nullptr)
        submit_and_wait(*command_pool->queue, fence);
}