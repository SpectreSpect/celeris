#include "point_cloud.h"

#include "../material_instance_manager.h"
#include "../mesh_manager.h"
#include "point_instance.h"


PointCloud::PointCloud(VulkanEngine& engine, Mesh& mesh, MaterialInstance& material)
    :   InstancedRenderObject(engine, mesh, material) {}

PointCloud::PointCloud(VulkanEngine& engine, MeshManager& mesh_manager, MaterialInstanceManager& material_instance_manager)
    :   PointCloud(engine, mesh_manager.point_cloud_quad, material_instance_manager.point_cloud) {}

PointCloud::PointCloud(ManagerBundle& manager_bundle)
    :   PointCloud(manager_bundle.engine(), manager_bundle.mesh_manager(), 
                   manager_bundle.material_instance_manager()) {}

PointCloud::PointCloud(VulkanEngine& engine, Mesh& mesh, MaterialInstance& material, uint32_t instance_count) 
    :   InstancedRenderObject(engine, mesh, material) {
    m_instance_batch = std::make_unique<InstanceBatch>(engine, instance_count, sizeof(PointInstance));
    set_instance_view(m_instance_batch->get_view());
}

PointCloud::PointCloud(VulkanEngine& engine, Mesh& mesh, MaterialInstance& material, InstanceBatch instance_batch)
    :   InstancedRenderObject(engine, mesh, material) {
    m_instance_batch = std::make_unique<InstanceBatch>(std::move(instance_batch));
    set_instance_view(m_instance_batch->get_view());
}

PointCloud::PointCloud(VulkanEngine& engine, MeshManager& mesh_manager, 
                       MaterialInstanceManager& material_instance_manager, InstanceBatch instance_batch)
    :   PointCloud(engine, mesh_manager.point_cloud_quad, 
                  material_instance_manager.point_cloud, std::move(instance_batch)) {}

PointCloud::PointCloud(ManagerBundle& manager_bundle, InstanceBatch instance_batch)
    :   PointCloud(manager_bundle.engine(), manager_bundle.mesh_manager(), 
                   manager_bundle.material_instance_manager(), std::move(instance_batch)) {}

PointCloud::PointCloud(VulkanEngine& engine, Mesh& mesh, MaterialInstance& material, 
                       VulkanBuffer& instance_buffer, uint32_t instance_count)
    :   InstancedRenderObject(engine, mesh, material) {
    set_instance_view(InstanceBufferView(instance_buffer, instance_count, sizeof(PointInstance)));
}

PointCloud::PointCloud(VulkanEngine& engine, MeshManager& mesh_manager, 
                       MaterialInstanceManager& material_instance_manager, VulkanBuffer& instance_buffer, uint32_t instance_count)
    :   PointCloud(engine, mesh_manager.point_cloud_quad, 
                   material_instance_manager.point_cloud, instance_buffer, instance_count) {}

PointCloud::PointCloud(ManagerBundle& manager_bundle, VulkanBuffer& instance_buffer, uint32_t instance_count)
    :   PointCloud(manager_bundle.engine(), manager_bundle.mesh_manager(), 
                   manager_bundle.material_instance_manager(), instance_buffer, instance_count) {}

PointCloud::PointCloud(VulkanEngine& engine, MeshManager& mesh_manager, 
                       MaterialInstanceManager& material_instance_manager, uint32_t instance_count) 
    :   PointCloud(engine, mesh_manager.point_cloud_quad, material_instance_manager.point_cloud, instance_count) {}

PointCloud::PointCloud(ManagerBundle& manager_bundle, uint32_t instance_count)
    :   PointCloud(manager_bundle.engine(), manager_bundle.mesh_manager(), 
                   manager_bundle.material_instance_manager(), instance_count) {}

PointCloud::PointCloud(ManagerBundle& manager_bundle, const std::vector<PointInstance>& points)
    :   PointCloud(manager_bundle.engine(), manager_bundle.mesh_manager(), 
                   manager_bundle.material_instance_manager(), points.size()) {
    set_points(points);
}

void PointCloud::set_points(const std::vector<PointInstance>& points) {
    LOG_METHOD();

    logger.check(!points.empty(), "Points vector was empty");

    m_instance_batch->set_instance_count(points.size());
    m_instance_batch->buffer().upload(points.data(), points.size() * sizeof(PointInstance));
    
    set_instance_view(m_instance_batch->get_view());
}

// bool PointCloud::has_owned_instance_batch() const {
//     return m_instance_batch != nullptr;
// }

// const InstanceBatch& PointCloud::owned_instance_batch() const {
//     LOG_METHOD();

//     logger.check(m_instance_batch != nullptr, "PointCloud does not own an InstanceBatch");
//     return *m_instance_batch;
// }

// InstanceBatch& PointCloud::owned_instance_batch() {
//     logger.check(m_instance_batch != nullptr, "PointCloud does not own an InstanceBatch");
//     return *m_instance_batch;
// }

// const VulkanBuffer& PointCloud::owned_instance_buffer() const {
//     LOG_METHOD();

//     logger.check(m_instance_batch != nullptr, "PointCloud does not own an InstanceBatch");
//     return m_instance_batch->buffer();
// }

// VulkanBuffer& PointCloud::owned_instance_buffer() {
//     LOG_METHOD();

//     logger.check(m_instance_batch != nullptr, "PointCloud does not own an InstanceBatch");
//     return m_instance_batch->buffer();
// }

// void PointCloud::set_instance_count(uint32_t instance_count) {
//     LOG_METHOD();

//     logger.check(m_instance_batch != nullptr, "PointCloud does not own an InstanceBatch");

//     m_instance_batch->set_instance_count(instance_count);
//     set_instance_view(m_instance_batch->get_view());
// }