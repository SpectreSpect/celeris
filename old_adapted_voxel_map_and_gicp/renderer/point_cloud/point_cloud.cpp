#include "point_cloud.h"

#include "../../../managers/material_instance_manager.h"
#include "../../../managers/mesh_manager.h"
#include "point_instance.h"


OldPointCloud::OldPointCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material)
    :   InstancedRenderObject(engine, mesh, material) {}

OldPointCloud::OldPointCloud(VulkanEngine& engine, MeshManager& mesh_manager, MaterialInstanceManager& material_instance_manager)
    :   OldPointCloud(engine, mesh_manager.point_cloud_quad, material_instance_manager.point_cloud) {}

OldPointCloud::OldPointCloud(ManagerBundle& manager_bundle)
    :   OldPointCloud(manager_bundle.engine(), manager_bundle.mesh_manager(), 
                   manager_bundle.material_instance_manager()) {}

OldPointCloud::OldPointCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material, uint32_t instance_count) 
    :   InstancedRenderObject(engine, mesh, material) {
    m_instance_batch = std::make_unique<InstanceBatch>(engine, instance_count, sizeof(OldPointInstance));
    set_instance_view(m_instance_batch->get_view());
}

OldPointCloud::OldPointCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material, InstanceBatch instance_batch)
    :   InstancedRenderObject(engine, mesh, material) {
    m_instance_batch = std::make_unique<InstanceBatch>(std::move(instance_batch));
    set_instance_view(m_instance_batch->get_view());
}

OldPointCloud::OldPointCloud(VulkanEngine& engine, MeshManager& mesh_manager, 
                       MaterialInstanceManager& material_instance_manager, InstanceBatch instance_batch)
    :   OldPointCloud(engine, mesh_manager.point_cloud_quad, 
                  material_instance_manager.point_cloud, std::move(instance_batch)) {}

OldPointCloud::OldPointCloud(ManagerBundle& manager_bundle, InstanceBatch instance_batch)
    :   OldPointCloud(manager_bundle.engine(), manager_bundle.mesh_manager(), 
                   manager_bundle.material_instance_manager(), std::move(instance_batch)) {}

OldPointCloud::OldPointCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material, 
                       VulkanBuffer& instance_buffer, uint32_t instance_count)
    :   InstancedRenderObject(engine, mesh, material) {
    set_instance_view(InstanceBufferView(instance_buffer, instance_count, sizeof(OldPointInstance)));
}

OldPointCloud::OldPointCloud(VulkanEngine& engine, MeshManager& mesh_manager, 
                       MaterialInstanceManager& material_instance_manager, VulkanBuffer& instance_buffer, uint32_t instance_count)
    :   OldPointCloud(engine, mesh_manager.point_cloud_quad, 
                   material_instance_manager.point_cloud, instance_buffer, instance_count) {}

OldPointCloud::OldPointCloud(ManagerBundle& manager_bundle, VulkanBuffer& instance_buffer, uint32_t instance_count)
    :   OldPointCloud(manager_bundle.engine(), manager_bundle.mesh_manager(), 
                   manager_bundle.material_instance_manager(), instance_buffer, instance_count) {}

OldPointCloud::OldPointCloud(VulkanEngine& engine, MeshManager& mesh_manager, 
                       MaterialInstanceManager& material_instance_manager, uint32_t instance_count) 
    :   OldPointCloud(engine, mesh_manager.point_cloud_quad, material_instance_manager.point_cloud, instance_count) {}

OldPointCloud::OldPointCloud(ManagerBundle& manager_bundle, uint32_t instance_count)
    :   OldPointCloud(manager_bundle.engine(), manager_bundle.mesh_manager(), 
                   manager_bundle.material_instance_manager(), instance_count) {}

OldPointCloud::OldPointCloud(ManagerBundle& manager_bundle, const std::vector<OldPointInstance>& points)
    :   OldPointCloud(manager_bundle.engine(), manager_bundle.mesh_manager(), 
                   manager_bundle.material_instance_manager(), points.size()) {
    set_points(points);
}

void OldPointCloud::set_points(const std::vector<OldPointInstance>& points) {
    LOG_METHOD();

    logger().check(!points.empty(), "Points vector was empty");

    set_instance_count(points.size());
    instance_buffer().upload(points);
}

void OldPointCloud::set_color(glm::vec4 color) {
    set_material_data(PointCloudMaterialData{.color = color});
}

uint32_t OldPointCloud::point_count() const noexcept {
    return m_instance_batch->instance_count();
}
