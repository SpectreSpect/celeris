#include <vulkan/vulkan.h>

namespace ShaderStages {
    constexpr VkShaderStageFlags vertex = VK_SHADER_STAGE_VERTEX_BIT;
    constexpr VkShaderStageFlags fragment = VK_SHADER_STAGE_FRAGMENT_BIT;
    constexpr VkShaderStageFlags compute = VK_SHADER_STAGE_COMPUTE_BIT;
    constexpr VkShaderStageFlags vertex_fragment = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    constexpr VkShaderStageFlags all_graphics = VK_SHADER_STAGE_ALL_GRAPHICS;
    constexpr VkShaderStageFlags all = VK_SHADER_STAGE_ALL;
}