#include "vulkan_device.h"

#include <utility>
#include <cstdint>
#include <algorithm>
#include <unordered_map>

VulkanDevice::VulkanDevice(const VulkanPhysicalDevice& physical_device) {
    LOG_METHOD();

    const QueueAllocation& queue_allocation = physical_device.queue_allocation();

    std::unordered_map<uint32_t, uint32_t> queue_counts_by_family;

    auto register_queue_locations = [&](const std::vector<QueueLocation>& locations) {
        for (const QueueLocation& location : locations) {
            uint32_t required_queue_count = location.queue_index + 1;

            auto& current_count = queue_counts_by_family[location.family_index];
            current_count = std::max(current_count, required_queue_count);
        }
    };

    register_queue_locations(queue_allocation.graphics);
    register_queue_locations(queue_allocation.present);
    register_queue_locations(queue_allocation.compute);
    register_queue_locations(queue_allocation.transfer);

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::vector<std::vector<float>> queue_priorities;

    queue_create_infos.reserve(queue_counts_by_family.size());
    queue_priorities.reserve(queue_counts_by_family.size());

    for (const auto& [family_index, queue_count] : queue_counts_by_family) {
        auto& priorities = queue_priorities.emplace_back(queue_count, 1.0f);

        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = family_index;
        queue_create_info.queueCount = queue_count;
        queue_create_info.pQueuePriorities = priorities.data();

        queue_create_infos.push_back(queue_create_info);
    }
    
    VkPhysicalDeviceFeatures supported_features{};
    vkGetPhysicalDeviceFeatures(physical_device.handle(), &supported_features);

    logger.check(
        supported_features.imageCubeArray == VK_TRUE,
        "Physical device does not support imageCubeArray"
    );
    logger.check(
        supported_features.multiDrawIndirect == VK_TRUE,
        "Physical device does not support multiDrawIndirect"
    );


    VkPhysicalDeviceFeatures device_features{};
    device_features.shaderFloat64 = VK_TRUE;
    device_features.imageCubeArray = VK_TRUE;
    device_features.multiDrawIndirect = VK_TRUE;

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = static_cast<uint32_t>(VulkanPhysicalDevice::device_extensions.size());
    create_info.ppEnabledExtensionNames = VulkanPhysicalDevice::device_extensions.data();

    VkResult result = vkCreateDevice(
        physical_device.handle(),
        &create_info,
        nullptr,
        &m_device
    );

    logger.check(result == VK_SUCCESS, "Failed to create logical device");

    load_device_functions();

    retrieve_queues(queue_allocation);
}

VulkanDevice::~VulkanDevice() {
    destroy();
}

void VulkanDevice::destroy() {
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    m_vkCmdPushDescriptorSetKHR = nullptr;

    m_graphics_queues.clear();
    m_present_queues.clear();
    m_compute_queues.clear();
    m_transfer_queues.clear();
}

VulkanDevice::VulkanDevice(VulkanDevice&& other) noexcept
    :   m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
        m_vkCmdPushDescriptorSetKHR(std::exchange(other.m_vkCmdPushDescriptorSetKHR, nullptr)),
        m_graphics_queues(std::move(other.m_graphics_queues)),
        m_present_queues(std::move(other.m_present_queues)),
        m_compute_queues(std::move(other.m_compute_queues)),
        m_transfer_queues(std::move(other.m_transfer_queues)) {}

VulkanDevice& VulkanDevice::operator=(VulkanDevice&& other) noexcept {
    if (this != &other) {
        destroy();

        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_vkCmdPushDescriptorSetKHR = std::exchange(other.m_vkCmdPushDescriptorSetKHR, nullptr);
        m_graphics_queues = std::move(other.m_graphics_queues);
        m_present_queues = std::move(other.m_present_queues);
        m_compute_queues = std::move(other.m_compute_queues);
        m_transfer_queues = std::move(other.m_transfer_queues);
    }

    return *this;
}

VkDevice VulkanDevice::handle() const noexcept {
    return m_device;
}

void VulkanDevice::wait_idle() {
    LOG_METHOD();

    if (m_device != VK_NULL_HANDLE) {
        VkResult result = vkDeviceWaitIdle(m_device);
        logger.check(result == VK_SUCCESS, "Failed to wait for logical device idle");
    }
}

const VulkanQueue& VulkanDevice::graphics_queue(uint32_t index) const {
    LOG_METHOD();
    return get_queue(m_graphics_queues, index, "No graphics queues available");
}

VulkanQueue& VulkanDevice::graphics_queue(uint32_t index) {
    return const_cast<VulkanQueue&>(std::as_const(*this).graphics_queue(index));
}

const VulkanQueue& VulkanDevice::present_queue(uint32_t index) const {
    LOG_METHOD();
    return get_queue(m_present_queues, index, "No present queues available");
}

VulkanQueue& VulkanDevice::present_queue(uint32_t index) {
    return const_cast<VulkanQueue&>(std::as_const(*this).present_queue(index));
}

const VulkanQueue& VulkanDevice::compute_queue(uint32_t index) const {
    LOG_METHOD();
    return get_queue(m_compute_queues, index, "No compute queues available");
}

VulkanQueue& VulkanDevice::compute_queue(uint32_t index) {
    return const_cast<VulkanQueue&>(std::as_const(*this).compute_queue(index));
}

const VulkanQueue& VulkanDevice::transfer_queue(uint32_t index) const {
    LOG_METHOD();
    return get_queue(m_transfer_queues, index, "No transfer queues available");
}

VulkanQueue& VulkanDevice::transfer_queue(uint32_t index) {
    return const_cast<VulkanQueue&>(std::as_const(*this).transfer_queue(index));
}

PFN_vkCmdPushDescriptorSetKHR VulkanDevice::cmd_push_descriptor_set_khr() const noexcept {
    return m_vkCmdPushDescriptorSetKHR;
}

void VulkanDevice::load_device_functions() {
    m_vkCmdPushDescriptorSetKHR =
        reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(
            vkGetDeviceProcAddr(m_device, "vkCmdPushDescriptorSetKHR")
        );

    logger.check(
        m_vkCmdPushDescriptorSetKHR != nullptr,
        "Failed to load vkCmdPushDescriptorSetKHR"
    );
}

void VulkanDevice::retrieve_queues(const QueueAllocation& queue_allocation) {
    LOG_METHOD();

    auto retrieve_queue = [&](
        std::vector<std::unique_ptr<VulkanQueue>>& queues,
        const std::vector<QueueLocation>& locations,
        VulkanQueueType type) 
    {
        queues.clear();
        queues.reserve(locations.size());

        for (const QueueLocation& location : locations) {
            queues.push_back(std::make_unique<VulkanQueue>(*this, location, type));
        }
    };

    retrieve_queue(m_graphics_queues, queue_allocation.graphics, VulkanQueueType::Graphics);
    retrieve_queue(m_present_queues,  queue_allocation.present,  VulkanQueueType::Present);
    retrieve_queue(m_compute_queues,  queue_allocation.compute,  VulkanQueueType::Compute);
    retrieve_queue(m_transfer_queues, queue_allocation.transfer, VulkanQueueType::Transfer);
}

const VulkanQueue& VulkanDevice::get_queue(
    const std::vector<std::unique_ptr<VulkanQueue>>& queues,
    uint32_t index,
    std::string_view error_message) const
{
    LOG_METHOD();
    logger.check(!queues.empty(), error_message.data());
    logger.check(index < queues.size(), "Queue index is out of range");

    return *queues[index];
}

VulkanQueue& VulkanDevice::get_queue(
    std::vector<std::unique_ptr<VulkanQueue>>& queues,
    uint32_t index,
    std::string_view error_message)
{
    return const_cast<VulkanQueue&>(std::as_const(*this).get_queue(queues, index, error_message));
}
