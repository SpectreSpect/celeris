#include "buffer_filler.h"

#include "vulkan_physical_device.h"
#include "vulkan_device.h"
#include "../renderer/compute_pass_manager.h"
#include "vulkan_command_buffer.h"
#include "vulkan_command_pool.h"
#include "vulkan_queue.h"

BufferFiller::BufferFiller(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    const VulkanCommandPool& command_pool,
    VulkanQueue& queue,
    ComputePassManager& compute_pass_manager, 
    VkDeviceSize initial_prifab_buffer_size,
    uint32_t count_fills_in_flight)
    :   m_physical_device(physical_device.handle()),
        m_device(device.handle()),
        m_queue(&queue),
        m_fill_resources(
            create_fill_resources(
                device,
                command_pool,
                compute_pass_manager,
                count_fills_in_flight
            )
        ),
        m_prifab_buffer(VulkanBuffer::create_storage_buffer(physical_device, device, initial_prifab_buffer_size))
    {
        LOG_METHOD();

        logger.check(m_physical_device != VK_NULL_HANDLE, "Physical device is not initialized");
        logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    }

VulkanBuffer& BufferFiller::fill_buffer(
    VulkanCommandBuffer& command_buffer,
    const void* data,
    uint32_t size_data_bytes,
    VulkanBuffer& dst_buffer,
    uint32_t offset_in_dst,
    uint32_t invocation_stride)
{
    LOG_METHOD();

    auto& resource = m_fill_resources[current_fill_resource_id]; 

    resource.m_fence.wait();

    resource.m_fence.reset();
    resource.m_command_buffer.reset();
    
    dst_buffer.fill(
        resource.m_command_buffer,
        resource.m_fill_pass_instance,
        m_prifab_buffer,
        data,
        size_data_bytes,
        offset_in_dst,
        invocation_stride
    );

    m_queue->submit(
        resource.m_command_buffer, 
        &resource.m_fence
    );

    current_fill_resource_id = (current_fill_resource_id + 1) % m_fill_resources.size();

    return dst_buffer;
}

std::vector<BufferFiller::FillResource> BufferFiller::create_fill_resources(
    const VulkanDevice& device,
    const VulkanCommandPool& command_pool,
    ComputePassManager& compute_pass_manager,
    size_t count_fill_recources)
{
    LOG_NAMED("BufferFiller");

    std::vector<FillResource> fill_resources;
    fill_resources.reserve(count_fill_recources);

    for (size_t i = 0; i < count_fill_recources; i++) {
        fill_resources.emplace_back(
            VulkanCommandBuffer(device, command_pool), 
            ComputePassInstance(compute_pass_manager.descriptor_pool(), compute_pass_manager.fill_buffer_cp),
            VulkanFence(device)
        );
    }

    return fill_resources;
}
