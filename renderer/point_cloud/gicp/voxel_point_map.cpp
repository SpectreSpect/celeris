#include "voxel_point_map.h"

#include "../point_instance.h"
#include "../../../vulkan_self/vulkan_engine.h"
#include "../../instance_buffer_view.h"
#include "../../../voxel_grid_vulkan/voxel_grid_structures.h"
#include "../../../voxel_grid_vulkan/voxel_grid.h"
#include "../../../vulkan_self/utils.h"

VoxelPointMap::VoxelPointMap(VulkanEngine& engine, uint32_t num_hash_table_slots, uint32_t max_map_point_count)
    :   m_num_hash_table_slots(num_hash_table_slots),
        m_max_map_point_count(max_map_point_count),
        map_uniform_buffer(VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(VoxelPointMapUniform))),
        map_hash_table_buffer(VulkanBuffer::create_storage_buffer(
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

    std::vector<PointInstance> point_map_points(
        m_map_point_count
    );

    std::vector<glm::vec4> point_map_normals(
        m_map_point_count
    );

    HashTableCountersGpu point_hash_table_counters;

    std::vector<IndexHashTableSlotGpu> point_hash_table(
        m_map_point_count
    );

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
        m_map_point_count * sizeof(IndexHashTableSlotGpu),
        sizeof(HashTableCountersGpu)
    );

    out.write(&m_map_point_count, 0);
    out.write(point_map_points.data(), sizeof(PointInstance) * m_map_point_count);
    out.write(point_map_normals.data(), sizeof(glm::vec4) * m_map_point_count);
    out.write(&point_hash_table_counters, sizeof(HashTableCountersGpu));
    out.write(point_hash_table.data(), sizeof(IndexHashTableSlotGpu) * m_map_point_count);
    
    save_file.close();
}

void VoxelPointMap::load(std::string path) {
    std::ifstream in(path, std::ios::binary);

    map_file >> m_map_point_count;

    std::vector<PointInstance> point_map_points(
        m_map_point_count
    );

    std::vector<glm::vec4> point_map_normals(
        m_map_point_count
    );

    for (int i = 0; i < m_map_point_count; i++) {
        PointInstance point_instance{};

        map_file >> point_instance.position.x;
        map_file >> point_instance.position.y;
        map_file >> point_instance.position.z;
        map_file >> point_instance.position.w;

        map_file >> point_instance.color.r;
        map_file >> point_instance.color.g;
        map_file >> point_instance.color.b;
        map_file >> point_instance.color.a;

        point_map_points.push_back(point_instance);
    }

    for (int i = 0; i < m_map_point_count; i++) {
        glm::vec4 normal{};

        map_file >> normal.x;
        map_file >> normal.y;
        map_file >> normal.z;
        map_file >> normal.w;

        point_map_normals.push_back(normal);
    }

    map_point_buffer.upload(
        point_map_points.data(), 
        m_map_point_count * sizeof(PointInstance),
        0
    );

    map_normal_buffer.upload(
        point_map_normals.data(), 
        m_map_point_count * sizeof(glm::vec4),
        0
    );

    // for (int i = 0; i < m_map_point_count; i++) {
    //     saved_file 
    //         << point_map_normals[i].x << "\n"
    //         << point_map_normals[i].y << "\n"
    //         << point_map_normals[i].z << "\n"
    //         << point_map_normals[i].w << "\n";
    // }
}