#pragma once

#include <vector>

#include "../instanced_render_object.h"
#include "../../managers/manager_bundle.h"
#include "../../vulkan_self/logger/logger.h"

class VulkanEngine;
class MeshManager;
class MaterialInstanceManager;
struct PointInstance;

class PointCloud : public InstancedRenderObject {
public:
    _XCHILD_NAME(PointCloud);

    PointCloud(VulkanEngine& engine, Mesh& mesh, MaterialInstance& material);
    PointCloud(VulkanEngine& engine, MeshManager& mesh_manager, MaterialInstanceManager& material_instance_manager);
    PointCloud(ManagerBundle& manager_bundle);

    PointCloud(VulkanEngine& engine, Mesh& mesh, MaterialInstance& material, InstanceBatch instance_batch);
    PointCloud(VulkanEngine& engine, MeshManager& mesh_manager, MaterialInstanceManager& material_instance_manager, InstanceBatch instance_batch);
    PointCloud(ManagerBundle& manager_bundle, InstanceBatch instance_batch);

    PointCloud(VulkanEngine& engine, Mesh& mesh, MaterialInstance& material, VulkanBuffer& instance_buffer, uint32_t instance_count);
    PointCloud(VulkanEngine& engine, MeshManager& mesh_manager, MaterialInstanceManager& material_instance_manager, VulkanBuffer& instance_buffer, uint32_t instance_count);
    PointCloud(ManagerBundle& manager_bundle, VulkanBuffer& instance_buffer, uint32_t instance_count);

    PointCloud(VulkanEngine& engine, Mesh& mesh, MaterialInstance& material, uint32_t instance_count);
    PointCloud(VulkanEngine& engine, MeshManager& mesh_manager, MaterialInstanceManager& material_instance_manager, uint32_t instance_count);
    PointCloud(ManagerBundle& manager_bundle, uint32_t instance_count);
    PointCloud(ManagerBundle& manager_bundle, const std::vector<PointInstance>& points);

    void set_points(const std::vector<PointInstance>& points);

    // bool has_owned_instance_batch() const;

    // const InstanceBatch& owned_instance_batch() const;
    // InstanceBatch& owned_instance_batch();

    // const VulkanBuffer& owned_instance_buffer() const;
    // VulkanBuffer& owned_instance_buffer();

private:
    std::unique_ptr<InstanceBatch> m_instance_batch;
};
