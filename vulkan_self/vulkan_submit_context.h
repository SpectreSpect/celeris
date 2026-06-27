#pragma once

#include "logger/logger_header.h"
#include "vulkan_command_pool.h"
#include "vulkan_command_buffer.h"
#include "vulkan_fence.h"

class VulkanDevice;
class VulkanQueue;
class VulkanSubmitContext;

class SubmitAndWaitScope {
public:
    _XCLASS_NAME(SubmitAndWaitScope);

    SubmitAndWaitScope() = delete;
    SubmitAndWaitScope(VulkanSubmitContext& submit_context);
    ~SubmitAndWaitScope() noexcept;

    SubmitAndWaitScope(const SubmitAndWaitScope&) = delete;
    SubmitAndWaitScope& operator=(const SubmitAndWaitScope&) = delete;
    SubmitAndWaitScope(SubmitAndWaitScope&&) = delete;
    SubmitAndWaitScope& operator=(SubmitAndWaitScope&&) = delete;

    const VulkanCommandBuffer& command_buffer() const noexcept;
    VulkanCommandBuffer& command_buffer() noexcept;

private:
    VulkanSubmitContext& m_submit_context;
};

class VulkanSubmitContext {
public:
    _XCLASS_NAME(VulkanSubmitContext);

    friend class SubmitAndWaitScope;

    VulkanSubmitContext(
        const VulkanDevice& device,
        VulkanQueue& queue
    );
    ~VulkanSubmitContext() noexcept = default;

    VulkanSubmitContext(const VulkanSubmitContext&) = delete;
    VulkanSubmitContext& operator=(const VulkanSubmitContext&) = delete;

    VulkanSubmitContext(VulkanSubmitContext&&) noexcept = default;
    VulkanSubmitContext& operator=(VulkanSubmitContext&&) noexcept = default;

    const VulkanQueue& queue() const; 
    VulkanQueue& queue();

    const VulkanCommandPool& command_pool() const noexcept;
    VulkanCommandPool& command_pool() noexcept;

    const VulkanCommandBuffer& command_buffer() const noexcept;
    VulkanCommandBuffer& command_buffer() noexcept;

    const VulkanFence& fence() const noexcept;
    VulkanFence& fence() noexcept;

    CommandBufferScope record_commands();

    void submit_and_wait();
    SubmitAndWaitScope submit_and_wait_scope();

private:
    VulkanQueue* m_queue = nullptr;

    VulkanCommandPool m_command_pool;
    VulkanCommandBuffer m_command_buffer;
    VulkanFence m_fence;

    // Здесь бы ещё добавить семафоры... #TODO
};
