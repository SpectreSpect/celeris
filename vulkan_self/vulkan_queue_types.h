#pragma once

#include <cstdint>
#include <vector>
#include <string>

enum class VulkanQueueType {
    Graphics,
    Present,
    Compute,
    Transfer
};

inline std::string queue_type_str(VulkanQueueType type) {
    switch(type) {
        case VulkanQueueType::Graphics: return "Graphics";
        case VulkanQueueType::Present: return "Present";
        case VulkanQueueType::Compute: return "Compute";
        case VulkanQueueType::Transfer: return "Transfer";
    }

    return "";
};

struct QueueRequest {
    uint32_t graphics_count = 1;
    uint32_t present_count = 1;
    uint32_t compute_count = 0;
    uint32_t transfer_count = 0;
};

struct QueueLocation {
    uint32_t family_index = 0;
    uint32_t queue_index = 0;

    bool operator==(const QueueLocation&) const = default;
};

struct QueueAllocation {
    std::vector<QueueLocation> graphics;
    std::vector<QueueLocation> present;
    std::vector<QueueLocation> compute;
    std::vector<QueueLocation> transfer;
};

struct QueueFamilyInfo {
    uint32_t queue_family_index = 0;
    VulkanQueueType queue_family_type = VulkanQueueType::Graphics;
};

