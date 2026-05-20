#include "vulkan_shader_module.h"

#include <fstream>
#include <utility>
#include <string>

#include "vulkan_device.h"

VulkanShaderModule::VulkanShaderModule(
    const VulkanDevice& device, 
    const std::filesystem::path& file_path) 
    :   m_device(device.handle()) 
{
    LOG_METHOD();

    std::vector<char> code = read_file(file_path);

    logger.check(!code.empty(), "Shader file is empty");
    logger.check(code.size() % 4 == 0, "Shader file size is not multiple of 4");

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkResult result = vkCreateShaderModule(
        m_device,
        &create_info,
        nullptr,
        &m_shader_module
    );

    logger.check(result == VK_SUCCESS, "Failed to create shader module");
}

VulkanShaderModule::~VulkanShaderModule() noexcept {
    destroy();
}

void VulkanShaderModule::destroy() noexcept {
    if (m_device != VK_NULL_HANDLE && m_shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_device, m_shader_module, nullptr);
    }

    m_device = VK_NULL_HANDLE;
    m_shader_module = VK_NULL_HANDLE;
}

VulkanShaderModule::VulkanShaderModule(VulkanShaderModule&& other) noexcept 
    :   m_shader_module(std::exchange(other.m_shader_module, VK_NULL_HANDLE)),
        m_device(std::exchange(other.m_device, VK_NULL_HANDLE)) {}

VulkanShaderModule& VulkanShaderModule::operator=(VulkanShaderModule&& other) noexcept {
    if (this != &other) {
        destroy();

        m_shader_module = std::exchange(other.m_shader_module, VK_NULL_HANDLE);
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    }

    return *this;
}

VkShaderModule VulkanShaderModule::handle() const noexcept {
    return m_shader_module;
}

std::vector<char> VulkanShaderModule::read_file(std::filesystem::path filename) {
    LOG_NAMED("VulkanShaderModule");

    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    logger.check(file.is_open())
        << "Failed to open file: "
        <<  clr(filename.filename().string(), LoggerPalette::blue) << "\n";

    size_t file_size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(file_size);

    file.seekg(0);
    file.read(buffer.data(), file_size);
    file.close();

    return buffer;
}
