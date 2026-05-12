#include "vulkan_fence.h"

#include "vulkan_device.h"
#include "collect_handles_helper.h"

#include <utility>
#include <string>

VulkanFence::VulkanFence(const VulkanDevice& device, VkFenceCreateFlags flags) : m_device(device.handle()) {
    LOG_METHOD();

    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = flags;

    VkResult result = vkCreateFence(
        m_device,
        &fence_info,
        nullptr,
        &m_fence
    );

    logger.check(result == VK_SUCCESS, "Failed to create a fence");
}

VulkanFence::~VulkanFence() noexcept {
    destroy();
}

void VulkanFence::destroy() noexcept {
    if (m_device != VK_NULL_HANDLE && m_fence != VK_NULL_HANDLE) {
        vkDestroyFence(m_device, m_fence, nullptr);
    }

    m_device = VK_NULL_HANDLE;
    m_fence = VK_NULL_HANDLE;
}

VulkanFence::VulkanFence(VulkanFence&& other) noexcept
    :   m_fence(std::exchange(other.m_fence, VK_NULL_HANDLE)),
        m_device(std::exchange(other.m_device, VK_NULL_HANDLE)) {}

VulkanFence& VulkanFence::operator=(VulkanFence&& other) noexcept {
    if (this != &other) {
        destroy();

        m_fence = std::exchange(other.m_fence, VK_NULL_HANDLE);
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    }

    return *this;
}

VkFence VulkanFence::handle() const noexcept {
    return m_fence;
}

void VulkanFence::check_correctness(std::span<const VkFence> fences) {
    LOG_NAMED("VulkanFence");

    logger.check(!fences.empty(), "Fence list is empty");

    for (size_t i = 0; i < fences.size(); i++) {
        if (fences[i] == VK_NULL_HANDLE) {
            logger.throw_error() 
                << "In a list of " << clr(std::to_string(fences.size()), LoggerPalette::orange) 
                << " fences, at least fence number " << clr(std::to_string(i), LoggerPalette::orange)
                << " was not initialized" << "\n";
        }
    }
}

void VulkanFence::check_device_consistency(VkDevice device, const std::vector<VulkanFence>& fences) {
    LOG_NAMED("VulkanFence");
    
    logger.check(device != VK_NULL_HANDLE, "Device is not initialized");

    for (const VulkanFence& fence : fences) {
        logger.check(
            fence.m_device == device,
            "Fence belongs to another device"
        );
    }
}

bool VulkanFence::wait(VkDevice device, std::span<const VkFence> fences, VkBool32 wait_all, uint64_t timeout) {
    LOG_NAMED("VulkanFence");

    logger.check(device != VK_NULL_HANDLE, "Device is not initialized");
    VulkanFence::check_correctness(fences);

    VkResult result = vkWaitForFences(
        device,
        static_cast<uint32_t>(fences.size()),
        fences.data(),
        wait_all,
        timeout
    );

    if (result == VK_TIMEOUT)
        return false;

    logger.check(result == VK_SUCCESS, "Failed to wait fences");
    return true;
}

bool VulkanFence::wait(uint64_t timeout) const {
    LOG_METHOD();

    return VulkanFence::wait(
        m_device,
        std::span<const VkFence>{&m_fence, 1},
        VK_TRUE,
        timeout
    );
}

void VulkanFence::reset(VkDevice device, std::span<const VkFence> fences) {
    LOG_NAMED("VulkanFence");

    logger.check(device != VK_NULL_HANDLE, "Device is not initialized");
    VulkanFence::check_correctness(fences);

    VkResult result = vkResetFences(device, static_cast<uint32_t>(fences.size()), fences.data());

    logger.check(result == VK_SUCCESS, "Failed to reset fences");
}

void VulkanFence::reset() {
    LOG_METHOD();
    
    VulkanFence::reset(
        m_device,
        std::span<const VkFence>{&m_fence, 1}
    );
}

bool VulkanFence::wait_fences(
    VkDevice device,
    const std::vector<VulkanFence>& fences,
    VkBool32 wait_all,
    uint64_t timeout)
{
    LOG_NAMED("VulkanFence");

    if (fences.empty())
        return true;

    VulkanFence::check_device_consistency(device, fences);

    std::vector<VkFence> fence_handles = collect_handles(fences);
    return VulkanFence::wait(device, fence_handles, wait_all, timeout);
}

bool VulkanFence::wait_fences(
    const VulkanDevice& device,
    const std::vector<VulkanFence>& fences,
    VkBool32 wait_all,
    uint64_t timeout)
{
    LOG_NAMED("VulkanFence");

    if (fences.empty())
        return true;

    return VulkanFence::wait_fences(device.handle(), fences, wait_all, timeout);
}

bool VulkanFence::wait_fences(
    const std::vector<VulkanFence>& fences,
    VkBool32 wait_all,
    uint64_t timeout)
{
    LOG_NAMED("VulkanFence");

    if (fences.empty())
        return true;

    return VulkanFence::wait_fences(fences[0].m_device, fences, wait_all, timeout);
}

void VulkanFence::reset_fences(VkDevice device, const std::vector<VulkanFence>& fences) {
    LOG_NAMED("VulkanFence");

    if (fences.empty())
        return;

    VulkanFence::check_device_consistency(device, fences);

    std::vector<VkFence> fence_handles = collect_handles(fences);
    VulkanFence::reset(device, fence_handles);
}

void VulkanFence::reset_fences(const VulkanDevice& device, const std::vector<VulkanFence>& fences) {
    LOG_NAMED("VulkanFence");

    if (fences.empty())
        return;

    reset_fences(device.handle(), fences);
}

void VulkanFence::reset_fences(const std::vector<VulkanFence>& fences) {
    LOG_NAMED("VulkanFence");

    if (fences.empty())
        return;

    reset_fences(fences[0].m_device, fences);
}

std::vector<VulkanFence> VulkanFence::create_fences(
    const VulkanDevice& device,
    uint32_t count,
    VkFenceCreateFlags flags)
{
    LOG_NAMED("VulkanFence");


    std::vector<VulkanFence> fences;
    if (count == 0)
        return fences;

    fences.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        fences.emplace_back(device, flags);
    }

    return fences;
}
