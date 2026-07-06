#include "voxel_point_map.h"

#include "../point_instance.h"
#include "../../../vulkan_self/vulkan_engine.h"
#include "../../instance_buffer_view.h"
#include "../../../voxel_grid_vulkan/voxel_grid_structures.h"
#include "../../../voxel_grid_vulkan/voxel_grid.h"
#include "../../../vulkan_self/utils.h"

#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {
struct VoxelPointMapFileHeader {
    char magic[8];
    uint32_t version;
    uint32_t point_count;
    uint32_t max_map_point_count;
    uint32_t hash_table_slot_count;
    uint32_t point_instance_size;
    uint32_t normal_size;
    uint32_t hash_table_counters_size;
    uint32_t hash_table_slot_size;
};

constexpr char VOXEL_POINT_MAP_MAGIC[8] = {'C', 'V', 'P', 'M', 'A', 'P', '0', '1'};
constexpr uint32_t VOXEL_POINT_MAP_VERSION = 1u;

template <class T>
void write_pod(std::ofstream& out, const T& value, const std::filesystem::path& path) {
    out.write(reinterpret_cast<const char*>(&value), static_cast<std::streamsize>(sizeof(T)));
    if (!out) {
        throw std::runtime_error("Failed to write: " + path.string());
    }
}

void write_bytes(std::ofstream& out, const void* data, size_t size_bytes, const std::filesystem::path& path) {
    if (size_bytes == 0) return;

    out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size_bytes));
    if (!out) {
        throw std::runtime_error("Failed to write: " + path.string());
    }
}

template <class T>
void read_pod(std::ifstream& in, T& value, const std::filesystem::path& path) {
    in.read(reinterpret_cast<char*>(&value), static_cast<std::streamsize>(sizeof(T)));
    if (!in) {
        throw std::runtime_error("Failed to read: " + path.string());
    }
}

void read_bytes(std::ifstream& in, void* data, size_t size_bytes, const std::filesystem::path& path) {
    if (size_bytes == 0) return;

    in.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size_bytes));
    if (!in) {
        throw std::runtime_error("Failed to read: " + path.string());
    }
}
}

VoxelPointMap::VoxelPointMap(VulkanEngine& engine, uint32_t num_hash_table_slots, uint32_t max_map_point_count)
    :   m_num_hash_table_slots(num_hash_table_slots),
        m_max_map_point_count(max_map_point_count),
        map_uniform_buffer(VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(VoxelPointMapUniform))),
        map_hash_table_buffer(VulkanBuffer::create_host_visible_storage_buffer(
            engine,
            sizeof(HashTableCountersGpu) + sizeof(IndexHashTableSlotGpu) * num_hash_table_slots
        )),
        // map_point_buffer(VulkanBuffer::create_storage_buffer(engine, sizeof(PointInstance) * max_map_point_count)),
        map_point_buffer(VulkanBuffer(engine.physical_device(), engine.device(), sizeof(PointInstance) * max_map_point_count, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)),        
        map_normal_buffer(VulkanBuffer::create_host_visible_storage_buffer(engine, sizeof(glm::vec4) * max_map_point_count)), 
        map_point_count_buffer(VulkanBuffer::create_host_visible_storage_buffer(engine, sizeof(uint32_t))){
}

InstanceBufferView VoxelPointMap::get_map_instance_view() {
    return InstanceBufferView(map_point_buffer, m_map_point_count, sizeof(PointInstance));
}

void VoxelPointMap::upload_voxels(VulkanEngine& engine, VoxelGrid& voxel_grid) {
    std::vector<PointInstance> points(m_map_point_count);

    map_point_buffer.read(points.data(), sizeof(PointInstance) * m_map_point_count, 0);

    std::vector<VoxelWriteGPU> voxel_writes;
    voxel_writes.reserve(m_map_point_count);

    for (int i = 0; i < points.size(); i++) {
        glm::ivec3 color{0, 98, 255};

        glm::vec3 local = glm::vec3(points[i].position) / glm::vec3(voxel_grid.voxel_size());

        glm::ivec4 voxel_pos = glm::ivec4(glm::floor(local.x),
                                          glm::floor(local.y),
                                          glm::floor(local.z),
                                          1);

        voxel_writes.push_back(
            VoxelWriteGPU{
                .world_voxel = voxel_pos,
                .voxel_data = VoxelDataGPU(1, VOXEL_VISABILITY_FLAG_BIT, color),
                .set_flags = OVERWRITE_BIT
            }
        ); 
    }

    VulkanBuffer voxel_write_list = VulkanBuffer::create_host_visible_storage_buffer(engine, sizeof(uint32_t) * 4 + Utils::size_bytes(voxel_writes));
    voxel_write_list.upload_scalar<uint32_t>(voxel_writes.size(), 0);
    voxel_write_list.upload(voxel_writes, sizeof(uint32_t) * 4);

    VulkanCommandBuffer compute_command_buffer(engine.device(), engine.compute_command_pool());
    {
        auto scope = compute_command_buffer.begin_scope();
        voxel_grid.set_voxels(compute_command_buffer, voxel_write_list);
    }
    VulkanFence compute_fence(engine.device());
    engine.compute_submit(compute_command_buffer, &compute_fence);
    compute_fence.wait();
}

uint32_t VoxelPointMap::map_point_count() const noexcept {
    return m_map_point_count;
}

void VoxelPointMap::save(const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Failed to open: " + path.string());

    if (m_map_point_count > m_max_map_point_count) {
        throw std::runtime_error("Voxel point map point count exceeds capacity");
    }

    VoxelPointMapFileHeader header{};
    std::memcpy(header.magic, VOXEL_POINT_MAP_MAGIC, sizeof(header.magic));
    header.version = VOXEL_POINT_MAP_VERSION;
    header.point_count = m_map_point_count;
    header.max_map_point_count = m_max_map_point_count;
    header.hash_table_slot_count = m_num_hash_table_slots;
    header.point_instance_size = sizeof(PointInstance);
    header.normal_size = sizeof(glm::vec4);
    header.hash_table_counters_size = sizeof(HashTableCountersGpu);
    header.hash_table_slot_size = sizeof(IndexHashTableSlotGpu);

    std::vector<PointInstance> point_map_points(m_map_point_count);
    std::vector<glm::vec4> point_map_normals(m_map_point_count);

    HashTableCountersGpu point_hash_table_counters;

    std::vector<IndexHashTableSlotGpu> point_hash_table(m_num_hash_table_slots);

    map_point_buffer.read(
        point_map_points.data(), 
        m_map_point_count * sizeof(PointInstance),
        0
    );

    map_normal_buffer.read(
        point_map_normals.data(), 
        m_map_point_count * sizeof(glm::vec4),
        0
    );

    map_hash_table_buffer.read(
        &point_hash_table_counters,
        sizeof(HashTableCountersGpu),
        0
    );

    map_hash_table_buffer.read(
        point_hash_table.data(),
        m_num_hash_table_slots * sizeof(IndexHashTableSlotGpu),
        sizeof(HashTableCountersGpu)
    );

    write_pod(out, header, path);
    write_bytes(out, point_map_points.data(), sizeof(PointInstance) * point_map_points.size(), path);
    write_bytes(out, point_map_normals.data(), sizeof(glm::vec4) * point_map_normals.size(), path);
    write_pod(out, point_hash_table_counters, path);
    write_bytes(out, point_hash_table.data(), sizeof(IndexHashTableSlotGpu) * point_hash_table.size(), path);
}

void VoxelPointMap::load(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Failed to open: " + path.string());

    VoxelPointMapFileHeader header{};
    read_pod(in, header, path);

    if (std::memcmp(header.magic, VOXEL_POINT_MAP_MAGIC, sizeof(header.magic)) != 0) {
        throw std::runtime_error("Invalid voxel point map file: " + path.string());
    }

    if (header.version != VOXEL_POINT_MAP_VERSION) {
        throw std::runtime_error("Unsupported voxel point map file version: " + path.string());
    }

    if (header.point_instance_size != sizeof(PointInstance) ||
        header.normal_size != sizeof(glm::vec4) ||
        header.hash_table_counters_size != sizeof(HashTableCountersGpu) ||
        header.hash_table_slot_size != sizeof(IndexHashTableSlotGpu)) {
        throw std::runtime_error("Voxel point map file layout does not match this build: " + path.string());
    }

    if (header.point_count > m_max_map_point_count) {
        throw std::runtime_error("Voxel point map file exceeds map point capacity: " + path.string());
    }

    if (header.hash_table_slot_count != m_num_hash_table_slots) {
        throw std::runtime_error("Voxel point map file hash table size does not match this map: " + path.string());
    }

    std::vector<PointInstance> point_map_points(header.point_count);
    std::vector<glm::vec4> point_map_normals(header.point_count);
    HashTableCountersGpu point_hash_table_counters;
    std::vector<IndexHashTableSlotGpu> point_hash_table(header.hash_table_slot_count);

    read_bytes(in, point_map_points.data(), sizeof(PointInstance) * point_map_points.size(), path);
    read_bytes(in, point_map_normals.data(), sizeof(glm::vec4) * point_map_normals.size(), path);
    read_pod(in, point_hash_table_counters, path);
    read_bytes(in, point_hash_table.data(), sizeof(IndexHashTableSlotGpu) * point_hash_table.size(), path);

    m_map_point_count = header.point_count;

    map_point_count_buffer.upload_scalar(m_map_point_count);

    if (!point_map_points.empty()) {
        map_point_buffer.upload(point_map_points);
        map_normal_buffer.upload(point_map_normals);
    }

    map_hash_table_buffer.upload(&point_hash_table_counters, sizeof(HashTableCountersGpu), 0);
    map_hash_table_buffer.upload(
        point_hash_table.data(),
        sizeof(IndexHashTableSlotGpu) * point_hash_table.size(),
        sizeof(HashTableCountersGpu)
    );
}
