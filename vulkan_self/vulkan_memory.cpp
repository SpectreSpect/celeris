#include "vulkan_memory.h"

#include <utility>
#include <string>
#include <cstring>
#include <cstddef>

#include "vulkan_physical_device.h"
#include "vulkan_device.h"
#include "vulkan_buffer.h"
#include "utils.h"

VulkanMemory::VulkanMemory(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    uint32_t memory_type_index,
    VkDeviceSize size_bytes)
{
    LOG_METHOD();
    allocate(physical_device, device, memory_type_index, size_bytes);
}

VulkanMemory::VulkanMemory(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    uint32_t memory_type_filter,
    VkMemoryPropertyFlags required_properties,
    VkDeviceSize size_bytes)
{
    LOG_METHOD();
    allocate(
        physical_device, 
        device, 
        find_memory_type(physical_device, memory_type_filter, required_properties, size_bytes), 
        size_bytes
    );
}

VulkanMemory::VulkanMemory(
    VkPhysicalDevice physical_device,
    VkDevice device,
    uint32_t memory_type_filter,
    VkMemoryPropertyFlags required_properties,
    VkDeviceSize size_bytes)
{
    LOG_METHOD();
    allocate(
        physical_device, 
        device, 
        find_memory_type(physical_device, memory_type_filter, required_properties, size_bytes), 
        size_bytes
    );
}

VulkanMemory::~VulkanMemory() noexcept {
    destroy();
}

void VulkanMemory::destroy() noexcept {
    if (m_device != VK_NULL_HANDLE && m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(
            m_device,
            m_memory,
            nullptr
        );
    }

    m_device = VK_NULL_HANDLE;
    m_memory = VK_NULL_HANDLE;
    m_size = 0;
    m_memory_type_index = 0;
    m_memory_properties = 0;
    m_non_coherent_atom_size = 1;
}

VulkanMemory::VulkanMemory(VulkanMemory&& other) noexcept
    :   m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
        m_memory(std::exchange(other.m_memory, VK_NULL_HANDLE)),
        m_size(std::exchange(other.m_size, 0)),
        m_memory_type_index(std::exchange(other.m_memory_type_index, 0)),
        m_memory_properties(std::exchange(other.m_memory_properties, 0)),
        m_non_coherent_atom_size(std::exchange(other.m_non_coherent_atom_size, 1)) {}

VulkanMemory& VulkanMemory::operator=(VulkanMemory&& other) noexcept {
    if (this != &other) {
        destroy();

        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_memory = std::exchange(other.m_memory, VK_NULL_HANDLE);
        m_size = std::exchange(other.m_size, 0);
        m_memory_type_index = std::exchange(other.m_memory_type_index, 0);
        m_memory_properties = std::exchange(other.m_memory_properties, 0);
        m_non_coherent_atom_size = std::exchange(other.m_non_coherent_atom_size, 1);
    }

    return *this;
}

VkDeviceMemory VulkanMemory::handle() const noexcept {
    return m_memory;
}

VkDeviceSize VulkanMemory::size() const noexcept {
    return m_size;
}

uint32_t VulkanMemory::memory_type_index() const noexcept {
    return m_memory_type_index;
}

VkMemoryPropertyFlags VulkanMemory::properties() const noexcept {
    return m_memory_properties;
}

bool VulkanMemory::is_host_visible() const noexcept {
    return (m_memory_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
}

bool VulkanMemory::is_host_coherent() const noexcept {
    return (m_memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
}

void VulkanMemory::map_memory(
    void*& mapped_memory,
    VkDeviceSize size_bytes,
    VkDeviceSize offset_bytes,
    VkMemoryMapFlags map_flags)
{
    LOG_METHOD();

    logger.check(m_memory != VK_NULL_HANDLE, "Memory is not initialized");
    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(size_bytes != 0, "Attempt to map zero bytes");
    logger.check(offset_bytes <= m_size, "Map offset is out of bounds");
    logger.check(size_bytes <= m_size - offset_bytes, "Mapped range out of bounds");
    logger.check(is_host_visible(), "Attempt to map to non-host-visible buffer memory");

    VkResult result = vkMapMemory(
        m_device,
        m_memory,
        offset_bytes,
        size_bytes,
        map_flags,
        &mapped_memory
    );

    logger.check(result == VK_SUCCESS, "Failed to map buffer memory");
}

void VulkanMemory::map_memory(void*& mapped_memory, VkMemoryMapFlags map_flags) {
    map_memory(mapped_memory, m_size, 0, map_flags);
}

void VulkanMemory::unmap_memory() {
    LOG_METHOD();

    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(m_memory != VK_NULL_HANDLE, "Buffer memory is not initialized");

    vkUnmapMemory(
        m_device,
        m_memory
    );
}

void VulkanMemory::flush(VkDeviceSize size_bytes, VkDeviceSize offset_bytes) const {
    LOG_METHOD();

    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(m_memory != VK_NULL_HANDLE, "Memory is not initialized");
    logger.check(size_bytes != 0, "Attempt to flush zero bytes");
    logger.check(offset_bytes <= m_size, "Flush offset is out of bounds");
    logger.check(size_bytes <= m_size - offset_bytes, "Flush range is out of bounds");

    if (is_host_coherent()) {
        return;
    }

    const VkDeviceSize atom_size = m_non_coherent_atom_size;

    VkDeviceSize flush_offset = Utils::align_down(offset_bytes, atom_size);
    VkDeviceSize flush_end = Utils::align_up(offset_bytes + size_bytes, atom_size);

    if (flush_end > m_size) {
        flush_end = m_size;
    }

    VkDeviceSize flush_size = flush_end - flush_offset;

    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = m_memory;
    range.offset = flush_offset;
    range.size = flush_size;

    VkResult result = vkFlushMappedMemoryRanges(
        m_device,
        1,
        &range
    );

    logger.check(result == VK_SUCCESS, "Failed to flush mapped memory range");
}

void VulkanMemory::flush() const {
    flush(m_size);
}

void VulkanMemory::upload(const void* data, VkDeviceSize size_bytes, VkDeviceSize offset_bytes) {
    LOG_METHOD();

    logger.check(data != nullptr, "Attempt to load data pointing to nullptr");
    logger.check(size_bytes != 0, "Attempt to upload zero bytes");
    logger.check(offset_bytes <= m_size, "Upload offset is out of bounds");
    logger.check(size_bytes <= m_size - offset_bytes, "Upload range is out of bounds");
    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(m_memory != VK_NULL_HANDLE, "Buffer memory is not initialized");
    logger.check(is_host_visible(), "Attempt to upload to non-host-visible buffer memory");

    void* mapped_data = nullptr;
    map_memory(mapped_data);

    std::memcpy(
        static_cast<std::byte*>(mapped_data) + offset_bytes,
        data,
        static_cast<size_t>(size_bytes)
    );

    flush(size_bytes, offset_bytes);
    
    unmap_memory();
}

void VulkanMemory::invalidate(VkDeviceSize size_bytes, VkDeviceSize offset_bytes) const {
    LOG_METHOD();

    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(m_memory != VK_NULL_HANDLE, "Memory is not initialized");
    logger.check(size_bytes != 0, "Attempt to invalidate zero bytes");
    logger.check(offset_bytes <= m_size, "Invalidate offset is out of bounds");
    logger.check(size_bytes <= m_size - offset_bytes, "Invalidate range is out of bounds");

    if (is_host_coherent()) {
        return;
    }

    const VkDeviceSize atom_size = m_non_coherent_atom_size;

    const VkDeviceSize end = offset_bytes + size_bytes;

    VkDeviceSize invalidate_offset = Utils::align_down(offset_bytes, atom_size);
    VkDeviceSize invalidate_end = Utils::align_up(end, atom_size);

    if (invalidate_end > m_size) {
        invalidate_end = m_size;
    }

    VkDeviceSize invalidate_size = invalidate_end - invalidate_offset;

    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = m_memory;
    range.offset = invalidate_offset;
    range.size = invalidate_size;

    VkResult result = vkInvalidateMappedMemoryRanges(
        m_device,
        1,
        &range
    );

    logger.check(result == VK_SUCCESS, "Failed to invalidate mapped memory range");
}

void VulkanMemory::invalidate() const {
    invalidate(m_size);
}

void VulkanMemory::read(void* data, VkDeviceSize size_bytes, VkDeviceSize offset_bytes) {
    LOG_METHOD();

    logger.check(data != nullptr, "Attempt to read data into nullptr");
    logger.check(size_bytes != 0, "Attempt to read zero bytes");
    logger.check(offset_bytes <= m_size, "Read offset is out of bounds");
    logger.check(size_bytes <= m_size - offset_bytes, "Read range is out of bounds");
    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(m_memory != VK_NULL_HANDLE, "Memory is not initialized");
    logger.check(is_host_visible(), "Attempt to read from non-host-visible memory");

    void* mapped_data = nullptr;
    map_memory(mapped_data);

    invalidate(size_bytes, offset_bytes);

    std::memcpy(
        data,
        static_cast<std::byte*>(mapped_data) + offset_bytes,
        static_cast<size_t>(size_bytes)
    );

    unmap_memory();
}

void VulkanMemory::bind_to_buffer(VulkanBuffer& buffer, VkDeviceSize memory_offset) const {
    LOG_METHOD();

    logger.check(m_memory != VK_NULL_HANDLE, "Memory is not initialized");
    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(buffer.handle() != VK_NULL_HANDLE, "Buffer is not initialized");

    VkResult result = vkBindBufferMemory(
        m_device,
        buffer.handle(),
        m_memory,
        memory_offset
    );

    logger.check(result == VK_SUCCESS, "Failed to bind buffer memory");
}

void VulkanMemory::bind_to_image(VkImage image, VkDeviceSize memory_offset) const {
    LOG_METHOD();

    logger.check(m_memory != VK_NULL_HANDLE, "Memory is not initialized");
    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(image != VK_NULL_HANDLE, "Image is not initialized");

    VkResult result = vkBindImageMemory(
        m_device,
        image,
        m_memory,
        memory_offset
    );

    logger.check(result == VK_SUCCESS, "Failed to bind image memory");
}

void VulkanMemory::allocate(
    VkPhysicalDevice physical_device,
    VkDevice device,
    uint32_t memory_type_index,
    VkDeviceSize size_bytes)
{
    LOG_METHOD();

    logger.check(physical_device != VK_NULL_HANDLE, "Physical device is not initialized");
    logger.check(device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(size_bytes != 0, "Attempt to allocate zero-sized Vulkan memory");

    m_device = device;
    m_size = size_bytes;
    m_memory_type_index = memory_type_index;
    m_memory_properties = get_memory_type_properties(physical_device, memory_type_index);

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(
        physical_device,
        &properties
    );

    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
    
    // logger.log() << "Allocated " 
    //     << clr(std::to_string(size_bytes), LoggerPalette::orange) 
    //     << " in heap "
    //     << clr(std::to_string(memory_properties.memoryTypes[m_memory_type_index].heapIndex), LoggerPalette::blue)
    //     << "\n";

    m_non_coherent_atom_size = properties.limits.nonCoherentAtomSize;

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = m_size;
    alloc_info.memoryTypeIndex = memory_type_index;

    VkResult result = vkAllocateMemory(
        m_device,
        &alloc_info,
        nullptr,
        &m_memory
    );

    logger.check(result == VK_SUCCESS, "Failed to allocate Vulkan memory");
}

void VulkanMemory::allocate(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    uint32_t memory_type_index,
    VkDeviceSize size_bytes)
{
    allocate(physical_device.handle(), device.handle(), memory_type_index, size_bytes);
}

/*
    В будущем нужно будет немного поменять принцип выбора памяти.
    Сейчас память выбиратся так, чтобы все требуемые свойтсва памяти присутсвовали,
    и было минимальное количество лишних свойств.

    Но суть в том, что иногда лишние свойства могут быть полезны. Это нужно будет пофиксить
    и учесть позже. #TODO
*/
uint32_t VulkanMemory::find_memory_type(
    VkPhysicalDevice physical_device,
    uint32_t type_filter,
    VkMemoryPropertyFlags required_properties,
    VkDeviceSize allocation_size)
{
    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

    uint32_t best_type = UINT32_MAX;
    uint32_t best_extra_flags_count = UINT32_MAX;
    VkDeviceSize best_heap_size = 0;

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        const VkMemoryType& memory_type = memory_properties.memoryTypes[i];
        const VkMemoryHeap& memory_heap =
            memory_properties.memoryHeaps[memory_type.heapIndex];

        bool type_supported =
            (type_filter & (1u << i)) != 0;

        bool has_required_properties =
            (memory_type.propertyFlags & required_properties) == required_properties;

        if (!type_supported || !has_required_properties) {
            continue;
        }

        if (allocation_size > 0 && memory_heap.size < allocation_size) {
            continue;
        }

        VkMemoryPropertyFlags extra_flags =
            memory_type.propertyFlags & ~required_properties;

        uint32_t extra_flags_count =
            Utils::count_bits(static_cast<uint32_t>(extra_flags));

        bool better =
            best_type == UINT32_MAX ||
            extra_flags_count < best_extra_flags_count ||
            (
                extra_flags_count == best_extra_flags_count &&
                memory_heap.size > best_heap_size
            );

        if (better) {
            best_type = i;
            best_extra_flags_count = extra_flags_count;
            best_heap_size = memory_heap.size;
        }
    }

    logger.check(best_type != UINT32_MAX, "Failed to find suitable Vulkan memory type");

    return best_type;
}

uint32_t VulkanMemory::find_memory_type(
    const VulkanPhysicalDevice& physical_device,
    uint32_t type_filter,
    VkMemoryPropertyFlags required_properties,
    VkDeviceSize allocation_size)
{
    return find_memory_type(physical_device.handle(), type_filter, required_properties, allocation_size);
}

VkMemoryPropertyFlags VulkanMemory::get_memory_type_properties(
    VkPhysicalDevice physical_device,
    uint32_t memory_type_index)
{
    LOG_NAMED("VulkanMemory");

    logger.check(physical_device != VK_NULL_HANDLE, "Physical device is not initialized");

    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(
        physical_device,
        &memory_properties
    );

    logger.check(memory_type_index < memory_properties.memoryTypeCount)
        << "Memory type index is out of range (memory_type_count = "
        << clr(std::to_string(memory_properties.memoryTypeCount), LoggerPalette::blue) << ")\n";
    
    return memory_properties.memoryTypes[memory_type_index].propertyFlags;
}

VkMemoryPropertyFlags VulkanMemory::get_memory_type_properties(
    const VulkanPhysicalDevice& physical_device,
    uint32_t memory_type_index) 
{
    return get_memory_type_properties(physical_device.handle(), memory_type_index);
}
