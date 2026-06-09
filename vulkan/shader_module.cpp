#include "shader_module.h"

VulkanShaderModule::VulkanShaderModule(VkDevice& device, const std::filesystem::path& path) {
    create(device, path);
}

VulkanShaderModule::~VulkanShaderModule() {
    destroy();
}

VulkanShaderModule::VulkanShaderModule(VulkanShaderModule&& other) noexcept {
    shader_module = other.shader_module;
    device = other.device;

    other.shader_module = VK_NULL_HANDLE;
    other.device = nullptr;
}

VulkanShaderModule& VulkanShaderModule::operator=(VulkanShaderModule&& other) noexcept {
    if (this != &other) {
        destroy();

        shader_module = other.shader_module;
        device = other.device;

        other.shader_module = VK_NULL_HANDLE;
        other.device = nullptr;
    }
    return *this;
}

void VulkanShaderModule::destroy() {
    if (device && shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(*device, shader_module, nullptr);
        shader_module = VK_NULL_HANDLE;
    }
    device = nullptr;
}

std::vector<char> VulkanShaderModule::read_file(const std::filesystem::path& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::string message = "failed to open file: " + filename.string();
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    file.close();

    return buffer;
}

void VulkanShaderModule::create(VkDevice& device, const std::vector<char>& code) {
    destroy();
    this->device = &device;

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    vulkan_utils::vk_check(
        vkCreateShaderModule(device, &createInfo, nullptr, &shader_module),
        "vkCreateShaderModule"
    );
}

void VulkanShaderModule::create(VkDevice& device, const std::filesystem::path& filename) {
    std::vector<char> code = read_file(filename);
    create(device, code);
}