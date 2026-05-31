#include "buffer_filler.h"

#include "vulkan_physical_device.h"
#include "vulkan_device.h"
#include "../renderer/compute_pass_manager.h"
#include "vulkan_command_buffer.h"

BufferFiller::BufferFiller(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    ComputePassManager& compute_pass_manager, 
    VkDeviceSize initial_prifab_buffer_size)
    :   m_prifab_buffer(VulkanBuffer::create_storage_buffer(physical_device, device, initial_prifab_buffer_size)),
        m_fill_pass_instance(compute_pass_manager.descriptor_pool(), compute_pass_manager.fill_buffer_cp) ,
        m_physical_device(physical_device.handle()),
        m_device(device.handle())
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

    dst_buffer.fill(
        command_buffer,
        m_fill_pass_instance,
        m_prifab_buffer,
        data,
        size_data_bytes,
        offset_in_dst,
        invocation_stride
    );

    return dst_buffer;
}
