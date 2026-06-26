#include "vulkan_physical_device.h"

#include <set>
#include <string>

VulkanPhysicalDevice::VulkanPhysicalDevice(
    const VulkanInstance& instance,
    const VulkanSurface& surface,
    const QueueRequest& queue_request)
    :   m_surface(surface.handle())
{
    LOG_METHOD();

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance.handle(), &device_count, nullptr);

    logger.check(device_count > 0, "Failed to find GPUs with Vulkan support");

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance.handle(), &device_count, devices.data());

    for (const auto& device : devices) {
        QueueAllocation candidate_allocation{};

        if (is_device_suitable(device, queue_request, &candidate_allocation)) {
            m_physical_device = device;
            m_queue_allocation = std::move(candidate_allocation);
            break;
        }
    }

    logger.check(
        m_physical_device != VK_NULL_HANDLE,
        "Failed to find a suitable GPU"
    );
}

VulkanPhysicalDevice::VulkanPhysicalDevice(VulkanPhysicalDevice&& other) noexcept
    :   m_physical_device(std::exchange(other.m_physical_device, VK_NULL_HANDLE)),
        m_surface(std::exchange(other.m_surface, VK_NULL_HANDLE)),
        m_queue_allocation(std::move(other.m_queue_allocation)) {}

VulkanPhysicalDevice& VulkanPhysicalDevice::operator=(VulkanPhysicalDevice&& other) noexcept {
    if (this != &other) {
        m_physical_device = std::exchange(other.m_physical_device, VK_NULL_HANDLE);
        m_surface = std::exchange(other.m_surface, VK_NULL_HANDLE);
        m_queue_allocation = std::move(other.m_queue_allocation);
    }

    return *this;
}

VkPhysicalDevice VulkanPhysicalDevice::handle() const noexcept {
    return m_physical_device;
}

const QueueAllocation& VulkanPhysicalDevice::queue_allocation() const noexcept {
    return m_queue_allocation;
}

bool VulkanPhysicalDevice::find_queue_families(
    VkPhysicalDevice device,
    const QueueRequest& queue_request,
    QueueAllocation* queue_allocation
) const
{
    LOG_METHOD();

    logger.check(queue_allocation != nullptr, "QueueAllocation pointer is null");

    *queue_allocation = QueueAllocation{};

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        device,
        &queue_family_count,
        queue_families.data()
    );

    std::vector<uint32_t> used_queue_counts(queue_family_count, 0);

    auto try_allocate_queues = [&](
        uint32_t family_index,
        uint32_t requested_count,
        std::vector<QueueLocation>& output
    ) -> bool {
        if (requested_count == 0) {
            return true;
        }

        if (!output.empty()) {
            return true;
        }

        const uint32_t used_count = used_queue_counts[family_index];
        const uint32_t available_count = queue_families[family_index].queueCount;

        if (used_count + requested_count > available_count) {
            return false;
        }

        for (uint32_t queue_index = used_count;
             queue_index < used_count + requested_count;
             ++queue_index) {
            output.emplace_back(family_index, queue_index);
        }

        used_queue_counts[family_index] += requested_count;

        return true;
    };

    for (uint32_t family_index = 0;
         family_index < queue_family_count;
         ++family_index) {
        const auto& queue_family = queue_families[family_index];

        VkBool32 present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(
            device,
            family_index,
            m_surface,
            &present_support
        );

        const bool supports_graphics = queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT;
        const bool supports_present = present_support == VK_TRUE;
        const bool supports_compute = queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT;
        const bool supports_transfer = queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT;

        if (queue_allocation->graphics.empty() && supports_graphics) {
            try_allocate_queues(
                family_index,
                queue_request.graphics_count,
                queue_allocation->graphics
            );
        }

        if (queue_allocation->present.empty() && supports_present) {
            try_allocate_queues(
                family_index,
                queue_request.present_count,
                queue_allocation->present
            );
        }

        if (queue_allocation->compute.empty() && supports_compute) {
            try_allocate_queues(
                family_index,
                queue_request.compute_count,
                queue_allocation->compute
            );
        }

        if (queue_allocation->transfer.empty() && supports_transfer) {
            try_allocate_queues(
                family_index,
                queue_request.transfer_count,
                queue_allocation->transfer
            );
        }
    }

    return
        queue_allocation->graphics.size() >= queue_request.graphics_count &&
        queue_allocation->present.size()  >= queue_request.present_count &&
        queue_allocation->compute.size()  >= queue_request.compute_count &&
        queue_allocation->transfer.size() >= queue_request.transfer_count;
}

bool VulkanPhysicalDevice::check_device_extension_support(VkPhysicalDevice device) const {
    LOG_METHOD();

    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(
        device,
        nullptr,
        &extension_count,
        available_extensions.data()
    );

    std::set<std::string> required_extensions(
        device_extensions.begin(),
        device_extensions.end()
    );

    for (const auto& extension : available_extensions) {
        required_extensions.erase(extension.extensionName);
    }

    return required_extensions.empty();
}

bool VulkanPhysicalDevice::check_device_extension_support() const {
    return check_device_extension_support(m_physical_device);
}

VulkanPhysicalDevice::SwapchainSupportDetails VulkanPhysicalDevice::query_swapchain_support(VkPhysicalDevice device) const {
    LOG_METHOD();

    SwapchainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        device,
        m_surface,
        &details.capabilities
    );

    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, nullptr);

    if (format_count != 0) {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            device,
            m_surface,
            &format_count,
            details.formats.data()
        );
    }

    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device,
        m_surface,
        &present_mode_count,
        nullptr
    );

    if (present_mode_count != 0) {
        details.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device,
            m_surface,
            &present_mode_count,
            details.present_modes.data()
        );
    }

    return details;
}

VulkanPhysicalDevice::SwapchainSupportDetails VulkanPhysicalDevice::query_swapchain_support() const {
    return query_swapchain_support(m_physical_device);
}

bool VulkanPhysicalDevice::is_device_suitable(
    VkPhysicalDevice device, 
    const QueueRequest& queue_request,
    QueueAllocation* queue_allocation) const 
{
    LOG_METHOD();

    bool queues_allocated = find_queue_families(device, queue_request, queue_allocation);

    bool extensions_supported = check_device_extension_support(device);

    bool swapchain_adequate = false;
    if (extensions_supported) {
        SwapchainSupportDetails swapchain_support = query_swapchain_support(device);
        swapchain_adequate =
            !swapchain_support.formats.empty() &&
            !swapchain_support.present_modes.empty();
    }

    return queues_allocated && extensions_supported && swapchain_adequate;
}
