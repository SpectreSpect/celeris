#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#define VK_CHECK(x)                                                                 \
    do {                                                                            \
        VkResult err__ = (x);                                                       \
        if (err__ != VK_SUCCESS) {                                                  \
            throw std::runtime_error(std::string("Vulkan error: ") + std::to_string(err__)); \
        }                                                                           \
    } while (0)

struct TestOutputCPU {
    double out_val[4];
};

static std::vector<uint32_t> read_spv(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error(std::string("Failed to open SPIR-V file: ") + path);
    }

    std::streamsize size = file.tellg();
    if (size <= 0 || (size % 4) != 0) {
        throw std::runtime_error("SPIR-V file size is invalid");
    }

    file.seekg(0, std::ios::beg);
    std::vector<uint32_t> data(static_cast<size_t>(size) / 4);
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        throw std::runtime_error("Failed to read SPIR-V file");
    }

    return data;
}

static uint32_t find_memory_type(
    VkPhysicalDevice physical_device,
    uint32_t type_bits,
    VkMemoryPropertyFlags required)
{
    VkPhysicalDeviceMemoryProperties mem_props{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_bits & (1u << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & required) == required) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

static void print_double_bits(const char* name, double value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));

    std::cout << std::setprecision(17);
    std::cout << name << " = " << value << "\n";
    std::cout << name << " bits = 0x"
              << std::hex << std::setw(16) << std::setfill('0') << bits
              << std::dec << "\n";
}

int main() {
    try {
        // ---------------- Instance ----------------
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "vk_compute_test";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "none";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo instance_info{};
        instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_info.pApplicationInfo = &app_info;

        VkInstance instance = VK_NULL_HANDLE;
        VK_CHECK(vkCreateInstance(&instance_info, nullptr, &instance));

        // ---------------- Physical device ----------------
        uint32_t physical_device_count = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr));
        if (physical_device_count == 0) {
            throw std::runtime_error("No Vulkan physical devices found");
        }

        std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data()));

        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        uint32_t compute_queue_family = UINT32_MAX;

        for (VkPhysicalDevice pd : physical_devices) {
            VkPhysicalDeviceFeatures features{};
            vkGetPhysicalDeviceFeatures(pd, &features);
            if (!features.shaderFloat64) {
                continue;
            }

            uint32_t queue_family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(pd, &queue_family_count, nullptr);
            std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(pd, &queue_family_count, queue_families.data());

            for (uint32_t i = 0; i < queue_family_count; ++i) {
                if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                    physical_device = pd;
                    compute_queue_family = i;
                    break;
                }
            }

            if (physical_device != VK_NULL_HANDLE) {
                break;
            }
        }

        if (physical_device == VK_NULL_HANDLE) {
            throw std::runtime_error("No suitable physical device with compute + shaderFloat64 found");
        }

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physical_device, &props);
        std::cout << "Using device: " << props.deviceName << "\n";

        // ---------------- Logical device ----------------
        float queue_priority = 1.0f;

        VkDeviceQueueCreateInfo queue_info{};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = compute_queue_family;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &queue_priority;

        VkPhysicalDeviceFeatures enabled_features{};
        enabled_features.shaderFloat64 = VK_TRUE;

        VkDeviceCreateInfo device_info{};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.queueCreateInfoCount = 1;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.pEnabledFeatures = &enabled_features;

        VkDevice device = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDevice(physical_device, &device_info, nullptr, &device));

        VkQueue queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, compute_queue_family, 0, &queue);

        // ---------------- Buffer ----------------
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory buffer_memory = VK_NULL_HANDLE;

        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = sizeof(TestOutputCPU);
        buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateBuffer(device, &buffer_info, nullptr, &buffer));

        VkMemoryRequirements mem_reqs{};
        vkGetBufferMemoryRequirements(device, buffer, &mem_reqs);

        uint32_t memory_type_index = find_memory_type(
            physical_device,
            mem_reqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = memory_type_index;

        VK_CHECK(vkAllocateMemory(device, &alloc_info, nullptr, &buffer_memory));
        VK_CHECK(vkBindBufferMemory(device, buffer, buffer_memory, 0));

        {
            void* mapped = nullptr;
            VK_CHECK(vkMapMemory(device, buffer_memory, 0, sizeof(TestOutputCPU), 0, &mapped));
            TestOutputCPU init{};
            init.out_val[0] = -1.0;
            init.out_val[1] = -1.0;
            init.out_val[2] = -1.0;
            init.out_val[3] = -1.0;
            std::memcpy(mapped, &init, sizeof(init));
            vkUnmapMemory(device, buffer_memory);
        }

        // ---------------- Descriptor set layout ----------------
        VkDescriptorSetLayoutBinding layout_binding{};
        layout_binding.binding = 0;
        layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        layout_binding.descriptorCount = 1;
        layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo dsl_info{};
        dsl_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dsl_info.bindingCount = 1;
        dsl_info.pBindings = &layout_binding;

        VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &dsl_info, nullptr, &descriptor_set_layout));

        VkPipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &descriptor_set_layout;

        VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
        VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout));

        // ---------------- Descriptor pool + set ----------------
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pool_size.descriptorCount = 1;

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.maxSets = 1;
        pool_info.poolSizeCount = 1;
        pool_info.pPoolSizes = &pool_size;

        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool));

        VkDescriptorSetAllocateInfo ds_alloc_info{};
        ds_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ds_alloc_info.descriptorPool = descriptor_pool;
        ds_alloc_info.descriptorSetCount = 1;
        ds_alloc_info.pSetLayouts = &descriptor_set_layout;

        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateDescriptorSets(device, &ds_alloc_info, &descriptor_set));

        VkDescriptorBufferInfo dbi{};
        dbi.buffer = buffer;
        dbi.offset = 0;
        dbi.range = sizeof(TestOutputCPU);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptor_set;
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pBufferInfo = &dbi;

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

        // ---------------- Shader module ----------------
        std::vector<uint32_t> spirv = read_spv("/home/spectre/Projects/test_open_3d/shader.comp.spv");

        VkShaderModuleCreateInfo shader_module_info{};
        shader_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_module_info.codeSize = spirv.size() * sizeof(uint32_t);
        shader_module_info.pCode = spirv.data();

        VkShaderModule shader_module = VK_NULL_HANDLE;
        VK_CHECK(vkCreateShaderModule(device, &shader_module_info, nullptr, &shader_module));

        // ---------------- Compute pipeline ----------------
        VkPipelineShaderStageCreateInfo stage_info{};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage_info.module = shader_module;
        stage_info.pName = "main";

        VkComputePipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.stage = stage_info;
        pipeline_info.layout = pipeline_layout;

        VkPipeline pipeline = VK_NULL_HANDLE;
        VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline));

        // ---------------- Command pool + command buffer ----------------
        VkCommandPoolCreateInfo cmd_pool_info{};
        cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmd_pool_info.queueFamilyIndex = compute_queue_family;

        VkCommandPool command_pool = VK_NULL_HANDLE;
        VK_CHECK(vkCreateCommandPool(device, &cmd_pool_info, nullptr, &command_pool));

        VkCommandBufferAllocateInfo cmd_alloc_info{};
        cmd_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_alloc_info.commandPool = command_pool;
        cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_alloc_info.commandBufferCount = 1;

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateCommandBuffers(device, &cmd_alloc_info, &cmd));

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            pipeline_layout,
            0,
            1,
            &descriptor_set,
            0,
            nullptr);

        vkCmdDispatch(cmd, 1, 1, 1);

        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_HOST_BIT,
            0,
            1, &barrier,
            0, nullptr,
            0, nullptr);

        VK_CHECK(vkEndCommandBuffer(cmd));

        // ---------------- Submit ----------------
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;

        VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(queue));

        // ---------------- Read back ----------------
        {
            void* mapped = nullptr;
            VK_CHECK(vkMapMemory(device, buffer_memory, 0, sizeof(TestOutputCPU), 0, &mapped));
            TestOutputCPU result{};
            std::memcpy(&result, mapped, sizeof(result));
            vkUnmapMemory(device, buffer_memory);

            print_double_bits("out_val.x", result.out_val[0]);
            print_double_bits("out_val.y", result.out_val[1]);
            print_double_bits("out_val.z", result.out_val[2]);
            print_double_bits("out_val.w", result.out_val[3]);
        }

        // ---------------- Cleanup ----------------
        vkDestroyCommandPool(device, command_pool, nullptr);
        vkDestroyPipeline(device, pipeline, nullptr);
        vkDestroyShaderModule(device, shader_module, nullptr);
        vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, buffer_memory, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}