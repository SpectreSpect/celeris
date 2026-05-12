#pragma once

#include <span>
#include <vector>
#include <cstdint>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"

class VulkanDevice;

class VulkanFence {
public:
    _XCLASS_NAME(VulkanFence);

    VulkanFence(const VulkanDevice& device, VkFenceCreateFlags flags = VK_FENCE_CREATE_SIGNALED_BIT);
    ~VulkanFence() noexcept;
    void destroy() noexcept;

    VulkanFence(const VulkanFence&) = delete;
    VulkanFence& operator=(const VulkanFence&) = delete;

    VulkanFence(VulkanFence&& other) noexcept;
    VulkanFence& operator=(VulkanFence&& other) noexcept;

    VkFence handle() const noexcept;

    bool wait(uint64_t timeout = UINT64_MAX) const;
    void reset();

    // Для удобного и надёжного использования (device передаётся явно, но через обёртку)
    static bool wait_fences(
        const VulkanDevice& device,
        const std::vector<VulkanFence>& fences,
        VkBool32 wait_all = VK_TRUE,
        uint64_t timeout = UINT64_MAX
    );

    // Девайс берётся из fences[0].m_device -> он должен быть корректно инициализирован (обычно верно)
    static bool wait_fences(
        const std::vector<VulkanFence>& fences,
        VkBool32 wait_all = VK_TRUE,
        uint64_t timeout = UINT64_MAX
    );

    static void reset_fences(const VulkanDevice& device, const std::vector<VulkanFence>& fences);
    static void reset_fences(const std::vector<VulkanFence>& fences);

    static std::vector<VulkanFence> create_fences(
        const VulkanDevice& device,
        uint32_t count,
        VkFenceCreateFlags flags = VK_FENCE_CREATE_SIGNALED_BIT
    );

private:
    VkFence m_fence = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

private:
    static void check_correctness(std::span<const VkFence> fences);
    static void check_device_consistency(VkDevice device, const std::vector<VulkanFence>& fences);

    static bool wait(
        VkDevice device,
        std::span<const VkFence> fences,
        VkBool32 wait_all,
        uint64_t timeout
    );

    static void reset(
        VkDevice device,
        std::span<const VkFence> fences
    );

    // Для реализации перегрузки без явной передачи device (так как fences[0].m_device имеет тип VkDevice)
    static bool wait_fences(
        VkDevice device,
        const std::vector<VulkanFence>& fences,
        VkBool32 wait_all = VK_TRUE,
        uint64_t timeout = UINT64_MAX
    );

    static void reset_fences(VkDevice device, const std::vector<VulkanFence>& fences);
};
