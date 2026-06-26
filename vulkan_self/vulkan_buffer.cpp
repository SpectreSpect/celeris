#include "vulkan_buffer.h"

#include "vulkan_command_buffer.h"
#include "vulkan_engine.h"
#include "../vulkan_self/pass/instance/pass_writer.h"
#include "utils.h"
#include "../math_utils.h"
#include "push_constants_structures.h"
#include "vulkan_buffer_view.h"

VulkanBuffer::VulkanBuffer(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkDeviceSize size_bytes,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memory_properties)
{
    LOG_METHOD();

    logger.check(physical_device.handle() != VK_NULL_HANDLE, "Physical device is not initialized");
    logger.check(device.handle() != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(size_bytes != 0, "Attempt to create a buffer with zero size");

    realloc(physical_device, device, size_bytes, usage, memory_properties);
}

VulkanBuffer::~VulkanBuffer() noexcept {
    destroy();
}

void VulkanBuffer::destroy_buffer() noexcept {
    vkDestroyBuffer(
        m_device,
        m_buffer,
        nullptr
    );
}

void VulkanBuffer::set_to_default_fields(bool except_memory) noexcept {
    m_physical_device = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_buffer = VK_NULL_HANDLE;
    m_size = 0;
    m_usage = 0;

    if (!except_memory)
        m_memory.reset();
}

void VulkanBuffer::destroy() noexcept {
    if (m_device != VK_NULL_HANDLE && m_buffer != VK_NULL_HANDLE) {
        destroy_buffer();
    }

    set_to_default_fields();    
}

VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) noexcept
    :   m_physical_device(std::exchange(other.m_physical_device, VK_NULL_HANDLE)),
        m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
        m_buffer(std::exchange(other.m_buffer, VK_NULL_HANDLE)),
        m_memory(std::move(other.m_memory)),
        m_size(std::exchange(other.m_size, 0)),
        m_usage(std::exchange(other.m_usage, 0)) {}

VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept
{
    if (this != &other) {
        destroy();

        m_physical_device = std::exchange(other.m_physical_device, VK_NULL_HANDLE);
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_buffer = std::exchange(other.m_buffer, VK_NULL_HANDLE);
        m_memory = std::move(other.m_memory);
        m_size = std::exchange(other.m_size, 0);
        m_usage = std::exchange(other.m_usage, 0);
    }

    return *this;
} 

VkBuffer VulkanBuffer::handle() const noexcept {
    return m_buffer;
}

VkDeviceSize VulkanBuffer::size() const noexcept {
    return m_size;
}

void VulkanBuffer::realloc(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkDeviceSize size_bytes,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memory_properties)
{
    LOG_METHOD();

    destroy();

    m_physical_device = physical_device;
    m_device = device;
    m_size = size_bytes;
    m_usage = usage;

    logger.check(m_physical_device != VK_NULL_HANDLE, "Physical device is not initialized");
    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(size_bytes != 0, "Attempt to create a buffer with zero size");

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size_bytes;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(
        m_device,
        &buffer_info,
        nullptr,
        &m_buffer
    );

    logger.check(result == VK_SUCCESS, "Failed to create buffer");

    try {
        VkMemoryRequirements memory_requirements{};
        vkGetBufferMemoryRequirements(
            m_device,
            m_buffer,
            &memory_requirements
        );

        m_memory.emplace(
            m_physical_device,
            m_device,
            memory_requirements.memoryTypeBits,
            memory_properties,
            memory_requirements.size
        );

        m_memory->bind_to_buffer(*this);
    } catch (...) {
        destroy_buffer();
        set_to_default_fields(true);
        throw;
    }
}

void VulkanBuffer::realloc(    
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkDeviceSize size_bytes,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memory_properties)
{
    realloc(
        physical_device.handle(),
        device.handle(),
        size_bytes,
        usage,
        memory_properties
    );
}

void VulkanBuffer::realloc(    
    VkDeviceSize size_bytes,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memory_properties) 
{
    realloc(m_physical_device, m_device, size_bytes, usage, memory_properties);
}

void VulkanBuffer::ensure_capacity(VkDeviceSize size_bytes) {
    LOG_METHOD();

    logger.check(m_physical_device != VK_NULL_HANDLE, "Physical device is not initialized");
    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(m_memory.has_value(), "Buffer memory is not initialized");

    if (m_size < size_bytes) {
        realloc(m_physical_device, m_device, size_bytes, m_usage, m_memory->properties());
    }
}

void VulkanBuffer::fill(VulkanCommandBuffer& command_buffer, uint32_t data) {
    LOG_METHOD();

    logger.check(m_buffer != VK_NULL_HANDLE, "Buffer is not initialized");
    logger.check(command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initialized");
    logger.check(has_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                 "Buffer must have VK_BUFFER_USAGE_TRANSFER_DST_BIT for vkCmdFillBuffer");

    vkCmdFillBuffer(command_buffer.handle(), m_buffer, 0, m_size, data);
}

void VulkanBuffer::fill(VulkanCommandBuffer& command_buffer, uint32_t data, VkDeviceSize size_bytes, VkDeviceSize offset) {
    LOG_METHOD();

    logger.check(m_buffer != VK_NULL_HANDLE, "Buffer is not initialized");
    logger.check(command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initialized");

    logger.check(has_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                "Buffer must have VK_BUFFER_USAGE_TRANSFER_DST_BIT for vkCmdFillBuffer");

    logger.check(size_bytes > 0, "Size bytes should be greater than 0");
    logger.check(offset <= m_size, "Offset was out of bounds");
    logger.check(size_bytes <= m_size - offset, "Fill range was out of bounds");

    logger.check((offset & 3ull) == 0, "vkCmdFillBuffer offset must be 4-byte aligned");
    logger.check((size_bytes & 3ull) == 0, "vkCmdFillBuffer size must be 4-byte aligned");

    vkCmdFillBuffer(command_buffer.handle(), m_buffer, offset, size_bytes, data);
}

VulkanBuffer& VulkanBuffer::fill(
    VulkanCommandBuffer& command_buffer,
    PassWriter& fill_pass_writer,
    const void* prefab,
    uint32_t prifab_size_bytes,
    uint32_t fillable_area_size_bytes,
    uint32_t fillable_area_offset,
    uint32_t invocation_stride) &
{
    LOG_METHOD();

    if (fillable_area_size_bytes == 0) {
        return *this;
    }

    logger.check(prefab != nullptr, "prifab must not be nullptr");
    logger.check(prifab_size_bytes != 0, "prifab_size_bytes must not be 0");
    logger.check(prifab_size_bytes <= PREFAB_MAX_UINTS * static_cast<uint32_t>(sizeof(uint32_t)), "The maximum size of the prefab has been exceeded");
    logger.check(invocation_stride != 0u, "invocation_stride must not be 0");
    logger.check(fillable_area_offset <= m_size, "Fillable area offset is out of bounds");
    logger.check(fillable_area_size_bytes <= m_size - fillable_area_offset, "Fill area exceeded the buffer size");
    logger.check(has_usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), "Destination buffer must have VK_BUFFER_USAGE_STORAGE_BUFFER_BIT");

    const uint32_t total_count_invocations = math_utils::div_up_u32(fillable_area_size_bytes, invocation_stride);

    uint32_t count_invocations_x = 0;
    uint32_t count_invocations_y = 0;
    uint32_t count_invocations_z = 0;

    if (total_count_invocations > 0u) {
        count_invocations_x = static_cast<uint32_t>(
            std::ceil(std::cbrt(static_cast<double>(total_count_invocations)))
        );

        const uint32_t count_invocations_yz = math_utils::div_up_u32(total_count_invocations, count_invocations_x);

        count_invocations_y = static_cast<uint32_t>(
            std::ceil(std::sqrt(static_cast<double>(count_invocations_yz)))
        );

        count_invocations_z = math_utils::div_up_u32(count_invocations_yz, count_invocations_y);
    }

    const uint32_t groups_x = math_utils::div_up_u32(count_invocations_x, FILL_LOCAL_SIZE_X);
    const uint32_t groups_y = math_utils::div_up_u32(count_invocations_y, FILL_LOCAL_SIZE_Y);
    const uint32_t groups_z = math_utils::div_up_u32(count_invocations_z, FILL_LOCAL_SIZE_Z);

    FillBufferPushConstants pc{
        .prefab_data_bytes = prifab_size_bytes,
        .clearable_data_bytes = fillable_area_size_bytes,

        .clearable_data_offset_bytes = fillable_area_offset,

        .count_invocations_x = count_invocations_x,
        .count_invocations_y = count_invocations_y,
        .count_invocations_z = count_invocations_z,

        .invocation_stride_bytes = invocation_stride
    };

    std::memcpy(pc.prefab_data, prefab, prifab_size_bytes);

    fill_pass_writer.set_storage_buffer(0, *this);
    fill_pass_writer.bind(command_buffer);
    fill_pass_writer.push_constants(command_buffer, pc);

    command_buffer.dispatch(groups_x, groups_y, groups_z);

    return *this;
}

VulkanBuffer&& VulkanBuffer::fill(
    VulkanCommandBuffer& command_buffer,
    PassWriter& fill_pass_writer,
    const void* prefab,
    uint32_t prifab_size_bytes,
    uint32_t fillable_area_size_bytes,
    uint32_t fillable_area_offset,
    uint32_t invocation_stride) &&
{
    static_cast<VulkanBuffer&>(*this).fill(
        command_buffer,
        fill_pass_writer,
        prefab,
        prifab_size_bytes,
        fillable_area_size_bytes,
        fillable_area_offset,
        invocation_stride
    );

    return std::move(*this);
}

void VulkanBuffer::upload(const void* data, VkDeviceSize size_bytes, VkDeviceSize offset_bytes) {
    LOG_METHOD();

    logger.check(m_buffer != VK_NULL_HANDLE, "Buffer is not initialized");
    logger.check(m_memory.has_value(), "Buffer memory is not initialized");
    logger.check(data != nullptr, "Attempt to upload data from nullptr");
    logger.check(size_bytes != 0, "Attempt to upload zero bytes");
    logger.check(offset_bytes <= m_size, "Upload offset is out of bounds");
    logger.check(size_bytes <= m_size - offset_bytes, "Upload range is out of bounds");

    m_memory->upload(data, size_bytes, offset_bytes);
}

void VulkanBuffer::read(void* data, VkDeviceSize size_bytes, VkDeviceSize offset_bytes) {
    LOG_METHOD();

    logger.check(m_buffer != VK_NULL_HANDLE, "Buffer is not initialized");
    logger.check(m_memory.has_value(), "Buffer memory is not initialized");
    logger.check(data != nullptr, "Attempt to read data into nullptr");
    logger.check(size_bytes != 0, "Attempt to read zero bytes");
    logger.check(offset_bytes <= size(), "Read offset is out of bounds");
    logger.check(size_bytes <= size() - offset_bytes, "Read range is out of bounds");

    m_memory->read(data, size_bytes, offset_bytes);
}

bool VulkanBuffer::has_usage(VkBufferUsageFlags usage) const noexcept {
    return (m_usage & usage) == usage;
}

bool VulkanBuffer::has_memory_property(VkMemoryPropertyFlags properties) const noexcept {
    return (m_memory->properties() & properties) == properties;
}

void VulkanBuffer::memory_barrier(
    VulkanCommandBuffer& command_buffer,
    VkPipelineStageFlags src_stage,
    VkAccessFlags src_access,
    VkPipelineStageFlags dst_stage,
    VkAccessFlags dst_access,
    VkDeviceSize offset_bytes,
    VkDeviceSize size_bytes) const
{
    LOG_METHOD();

    logger.check(command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initialized");
    logger.check(m_buffer != VK_NULL_HANDLE, "Buffer is not initialized");

    logger.check(src_stage != 0, "Source pipeline stage mask is empty");
    logger.check(dst_stage != 0, "Destination pipeline stage mask is empty");

    logger.check(offset_bytes <= size(), "Barrier offset is out of bounds");

    if (size_bytes != VK_WHOLE_SIZE) {
        logger.check(size_bytes != 0, "Attempt to create barrier for zero bytes");
        logger.check(size_bytes <= size() - offset_bytes, "Barrier range is out of bounds");
    }

    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = m_buffer;
    barrier.offset = offset_bytes;
    barrier.size = size_bytes;

    vkCmdPipelineBarrier(
        command_buffer.handle(),
        src_stage,
        dst_stage,
        0,
        0,
        nullptr,
        1,
        &barrier,
        0,
        nullptr
    );
}

void VulkanBuffer::memory_barrier_compute_write_to_compute_write_read(VulkanCommandBuffer& command_buffer) const {
    memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
    );
}

void VulkanBuffer::transfer_write_to_vertex_read_barrier(
    VulkanCommandBuffer& command_buffer,
    VkDeviceSize offset_bytes,
    VkDeviceSize size_bytes) const 
{
    LOG_METHOD();

    logger.check(
        has_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        "Buffer was not created with VK_BUFFER_USAGE_TRANSFER_DST_BIT"
    );

    logger.check(
        has_usage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
        "Buffer was not created with VK_BUFFER_USAGE_VERTEX_BUFFER_BIT"
    );

    memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
        offset_bytes,
        size_bytes
    );
}

void VulkanBuffer::transfer_write_to_compute_read_write_barrier(
    VulkanCommandBuffer& command_buffer,
    VkDeviceSize offset_bytes,
    VkDeviceSize size_bytes
) const {
    LOG_METHOD();

    logger.check(
        has_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        "Buffer was not created with VK_BUFFER_USAGE_TRANSFER_DST_BIT"
    );

    logger.check(
        has_usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        "Buffer was not created with VK_BUFFER_USAGE_STORAGE_BUFFER_BIT"
    );

    memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        offset_bytes,
        size_bytes
    );
}

void VulkanBuffer::compute_write_to_fragment_read_barrier(
    VulkanCommandBuffer& command_buffer,
    VkDeviceSize offset_bytes,
    VkDeviceSize size_bytes
) const {
    LOG_METHOD();

    logger.check(
        has_usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        "Buffer was not created with VK_BUFFER_USAGE_STORAGE_BUFFER_BIT"
    );

    memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        offset_bytes,
        size_bytes
    );
}

void VulkanBuffer::copy_to(
    VulkanCommandBuffer& command_buffer,
    VulkanBuffer& dst_buffer,
    VkDeviceSize size_bytes,
    VkDeviceSize src_offset_bytes,
    VkDeviceSize dst_offset_bytes
) const {
    LOG_METHOD();

    logger.check(command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initialized");

    logger.check(m_buffer != VK_NULL_HANDLE, "Source buffer is not initialized");
    logger.check(dst_buffer.handle() != VK_NULL_HANDLE, "Destination buffer is not initialized");

    logger.check(
        has_usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
        "Source buffer was not created with VK_BUFFER_USAGE_TRANSFER_SRC_BIT"
    );

    logger.check(
        dst_buffer.has_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        "Destination buffer was not created with VK_BUFFER_USAGE_TRANSFER_DST_BIT"
    );

    logger.check(size_bytes != 0, "Attempt to copy zero bytes");

    logger.check(src_offset_bytes <= size(), "Source copy offset is out of bounds");
    logger.check(size_bytes <= size() - src_offset_bytes, "Source copy range is out of bounds");

    logger.check(dst_offset_bytes <= dst_buffer.size(), "Destination copy offset is out of bounds");
    logger.check(size_bytes <= dst_buffer.size() - dst_offset_bytes, "Destination copy range is out of bounds");

    VkBufferCopy copy_region{};
    copy_region.srcOffset = src_offset_bytes;
    copy_region.dstOffset = dst_offset_bytes;
    copy_region.size = size_bytes;

    vkCmdCopyBuffer(
        command_buffer.handle(),
        m_buffer,
        dst_buffer.handle(),
        1,
        &copy_region
    );
}

void VulkanBuffer::bind_as_vertex_buffer(
    VulkanCommandBuffer& command_buffer,
    uint32_t buffer_binding,
    VkDeviceSize offset) const 
{
    LOG_METHOD();

    logger.check(m_buffer != VK_NULL_HANDLE, "Buffer is not initialized");
    logger.check(
        (m_usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) != 0,
        "Attempt to bind buffer as vertex buffer, but it was not created with VK_BUFFER_USAGE_VERTEX_BUFFER_BIT"
    );
    logger.check(command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initialized");

    // Позже сделать возможность использовать несколько биндингов. #TODO
    vkCmdBindVertexBuffers(command_buffer.handle(), buffer_binding, 1, &m_buffer, &offset);
}

void VulkanBuffer::bind_as_index_buffer(
    VulkanCommandBuffer& command_buffer,
    VkDeviceSize offset,
    VkIndexType index_type) const
{
    LOG_METHOD();

    logger.check(m_buffer != VK_NULL_HANDLE, "Buffer is not initialized");
    logger.check(
        (m_usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) != 0,
        "Attempt to bind buffer as index buffer, but it was not created with VK_BUFFER_USAGE_INDEX_BUFFER_BIT"
    );
    logger.check(command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initialized");

    // Позже сделать возможность использовать несколько биндингов. #TODO
    vkCmdBindIndexBuffer(command_buffer.handle(), m_buffer, offset, index_type);
}

VulkanBuffer VulkanBuffer::create_vertex_buffer(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkDeviceSize size_bytes) 
{
    LOG_NAMED("VulkanBuffer");

    logger.check(physical_device.handle() != VK_NULL_HANDLE, "Physical device is not initialized");
    logger.check(device.handle() != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(size_bytes != 0, "Attempt to create buffer with zero size");

    return VulkanBuffer(
        physical_device, 
        device,
        size_bytes,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
}

VulkanBuffer VulkanBuffer::create_vertex_buffer(
    const VulkanEngine& engine,
    VkDeviceSize size_bytes)
{
    return create_vertex_buffer(
        engine.physical_device(), 
        engine.device(), 
        size_bytes
    );
}

VulkanBuffer VulkanBuffer::create_staging_buffer(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkDeviceSize size_bytes) 
{
    LOG_NAMED("VulkanBuffer");

    logger.check(physical_device.handle() != VK_NULL_HANDLE, "Physical device is not initialized");
    logger.check(device.handle() != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(size_bytes != 0, "Attempt to create buffer with zero size");

    return VulkanBuffer(
        physical_device, 
        device,
        size_bytes,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
}

VulkanBuffer VulkanBuffer::create_staging_buffer(
    const VulkanEngine& engine,
    VkDeviceSize size_bytes)
{
    return create_staging_buffer(
        engine.physical_device(), 
        engine.device(), 
        size_bytes
    );
}

VulkanBuffer VulkanBuffer::create_index_buffer(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkDeviceSize size_bytes)
{
    LOG_NAMED("VulkanBuffer");

    logger.check(physical_device.handle() != VK_NULL_HANDLE, "Physical device is not initialized");
    logger.check(device.handle() != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(size_bytes != 0, "Attempt to create buffer with zero size");

    return VulkanBuffer(
        physical_device, 
        device,
        size_bytes,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
}

VulkanBuffer VulkanBuffer::create_index_buffer(
    const VulkanEngine& engine,
    VkDeviceSize size_bytes) 
{
    return create_index_buffer(
        engine.physical_device(), 
        engine.device(), 
        size_bytes
    );
}

VulkanBuffer VulkanBuffer::create_host_visible_storage_index_buffer(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkDeviceSize size_bytes)
{
    LOG_NAMED("VulkanBuffer");

    logger.check(physical_device.handle() != VK_NULL_HANDLE, "Physical device is not initialized");
    logger.check(device.handle() != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(size_bytes != 0, "Attempt to create buffer with zero size");

    return VulkanBuffer(
        physical_device,
        device,
        size_bytes,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
}

VulkanBuffer VulkanBuffer::create_host_visible_storage_index_buffer(
    const VulkanEngine& engine,
    VkDeviceSize size_bytes)
{
    return create_host_visible_storage_index_buffer(
        engine.physical_device(),
        engine.device(),
        size_bytes
    );
}

VulkanBuffer VulkanBuffer::create_storage_buffer(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkDeviceSize size_bytes)
{
    LOG_NAMED("VulkanBuffer");

    logger.check(physical_device.handle() != VK_NULL_HANDLE, "Physical device is not initialized");
    logger.check(device.handle() != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(size_bytes != 0, "Attempt to create buffer with zero size");

    return VulkanBuffer(
        physical_device, 
        device,
        size_bytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
}

VulkanBuffer VulkanBuffer::create_storage_buffer(
    const VulkanEngine& engine,
    VkDeviceSize size_bytes)
{
    return create_storage_buffer(
        engine.physical_device(), 
        engine.device(), 
        size_bytes
    );
}

VulkanBuffer VulkanBuffer::create_host_visible_uniform_buffer(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkDeviceSize size_bytes) 
{
    LOG_NAMED("VulkanBuffer");

    logger.check(physical_device.handle() != VK_NULL_HANDLE, "Physical device is not initialized");
    logger.check(device.handle() != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(size_bytes != 0, "Attempt to create buffer with zero size");

    return VulkanBuffer(
        physical_device, 
        device,
        size_bytes,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
}

VulkanBuffer VulkanBuffer::create_host_visible_uniform_buffer(
    const VulkanEngine& engine,
    VkDeviceSize size_bytes)
{
    return create_host_visible_uniform_buffer(
        engine.physical_device(), 
        engine.device(), 
        size_bytes
    );
}

VulkanBuffer VulkanBuffer::create_host_visible_storage_buffer(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkDeviceSize size_bytes) {
    LOG_NAMED("VulkanBuffer");

    logger.check(physical_device.handle() != VK_NULL_HANDLE, "Physical device is not initialized");
    logger.check(device.handle() != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(size_bytes != 0, "Attempt to create buffer with zero size");

    return VulkanBuffer(
        physical_device, 
        device,
        size_bytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
}

VulkanBuffer VulkanBuffer::create_host_visible_storage_buffer(
        const VulkanEngine& engine,
        VkDeviceSize size_bytes) {
    return create_host_visible_storage_buffer(
        engine.physical_device(), 
        engine.device(), 
        size_bytes
    );
}

VulkanBuffer VulkanBuffer::create_host_visible_indirect_storage_buffer(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkDeviceSize size_bytes
) {
    LOG_NAMED("VulkanBuffer");

    logger.check(
        physical_device.handle() != VK_NULL_HANDLE,
        "Physical device is not initialized"
    );

    logger.check(
        device.handle() != VK_NULL_HANDLE,
        "Device is not initialized"
    );

    logger.check(
        size_bytes != 0,
        "Attempt to create buffer with zero size"
    );

    return VulkanBuffer(
        physical_device,
        device,
        size_bytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
}

VulkanBuffer VulkanBuffer::create_host_visible_indirect_storage_buffer(
    const VulkanEngine& engine,
    VkDeviceSize size_bytes
) {
    return create_host_visible_indirect_storage_buffer(
        engine.physical_device(),
        engine.device(),
        size_bytes
    );
}

VulkanBuffer VulkanBuffer::create_host_visible_transfer_dst_storage_buffer(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkDeviceSize size_bytes
) {
    LOG_NAMED("VulkanBuffer");

    logger.check(
        physical_device.handle() != VK_NULL_HANDLE,
        "Physical device is not initialized"
    );

    logger.check(
        device.handle() != VK_NULL_HANDLE,
        "Device is not initialized"
    );

    logger.check(
        size_bytes != 0,
        "Attempt to create buffer with zero size"
    );

    return VulkanBuffer(
        physical_device,
        device,
        size_bytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
}

VulkanBuffer VulkanBuffer::create_host_visible_transfer_dst_storage_buffer(
    const VulkanEngine& engine,
    VkDeviceSize size_bytes
) {
    return create_host_visible_transfer_dst_storage_buffer(
        engine.physical_device(),
        engine.device(),
        size_bytes
    );
}


VulkanBuffer VulkanBuffer::create_host_visible_vertex_buffer(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkDeviceSize size_bytes)
{
    LOG_NAMED("VulkanBuffer");

    logger.check(physical_device.handle() != VK_NULL_HANDLE, "Physical device is not initialized");
    logger.check(device.handle() != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(size_bytes != 0, "Attempt to create buffer with zero size");

    return VulkanBuffer(
        physical_device,
        device,
        size_bytes,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
}

VulkanBuffer VulkanBuffer::create_host_visible_vertex_buffer(
    const VulkanEngine& engine,
    VkDeviceSize size_bytes)
{
    return create_host_visible_vertex_buffer(
        engine.physical_device(),
        engine.device(),
        size_bytes
    );
}

// .cpp
VulkanBuffer VulkanBuffer::create_host_visible_storage_vertex_buffer(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkDeviceSize size_bytes)
{
    LOG_NAMED("VulkanBuffer");

    logger.check(physical_device.handle() != VK_NULL_HANDLE, "Physical device is not initialized");
    logger.check(device.handle() != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(size_bytes != 0, "Attempt to create buffer with zero size");

    return VulkanBuffer(
        physical_device,
        device,
        size_bytes,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
}

VulkanBuffer VulkanBuffer::create_host_visible_storage_vertex_buffer(
    const VulkanEngine& engine,
    VkDeviceSize size_bytes)
{
    return create_host_visible_storage_vertex_buffer(
        engine.physical_device(),
        engine.device(),
        size_bytes
    );
}

VulkanBufferView VulkanBuffer::get_view() noexcept {
    return VulkanBufferView(*this);
}
