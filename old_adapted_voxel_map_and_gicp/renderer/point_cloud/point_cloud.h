#pragma once

#include <vector>

#include "../../../renderer/instanced_render_object.h"
#include "../../../managers/manager_bundle.h"
#include "../../../vulkan_self/logger/logger.h"
#include "point_instance.h"

class VulkanEngine;
class MeshManager;
class MaterialInstanceManager;
class OldPointCloud : public InstancedRenderObject {
public:
    _XCHILD_NAME(OldPointCloud);

    OldPointCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material);
    OldPointCloud(VulkanEngine& engine, MeshManager& mesh_manager, MaterialInstanceManager& material_instance_manager);
    OldPointCloud(ManagerBundle& manager_bundle);

    OldPointCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material, InstanceBatch instance_batch);
    OldPointCloud(VulkanEngine& engine, MeshManager& mesh_manager, MaterialInstanceManager& material_instance_manager, InstanceBatch instance_batch);
    OldPointCloud(ManagerBundle& manager_bundle, InstanceBatch instance_batch);

    OldPointCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material, VulkanBuffer& instance_buffer, uint32_t instance_count);
    OldPointCloud(VulkanEngine& engine, MeshManager& mesh_manager, MaterialInstanceManager& material_instance_manager, VulkanBuffer& instance_buffer, uint32_t instance_count);
    OldPointCloud(ManagerBundle& manager_bundle, VulkanBuffer& instance_buffer, uint32_t instance_count);

    OldPointCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material, uint32_t instance_count);
    OldPointCloud(VulkanEngine& engine, MeshManager& mesh_manager, MaterialInstanceManager& material_instance_manager, uint32_t instance_count);
    OldPointCloud(ManagerBundle& manager_bundle, uint32_t instance_count);
    OldPointCloud(ManagerBundle& manager_bundle, const std::vector<OldPointInstance>& points);

    uint32_t point_count() const noexcept;

    void set_points(const std::vector<OldPointInstance>& points);
    void set_color(glm::vec4 color);

private:
    std::unique_ptr<InstanceBatch> m_instance_batch;
};
