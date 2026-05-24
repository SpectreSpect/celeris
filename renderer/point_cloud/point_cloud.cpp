#include "point_cloud.h"

#include "../material_instance_manager.h"
#include "../mesh_manager.h"
#include "point_instance.h"


PointCloud::PointCloud(VulkanEngine& engine, Mesh& mesh, MaterialInstance& material, 
                       uint32_t instance_count, uint32_t instance_size_bytes) 
    :   InstancedRenderObject(engine, mesh, material, instance_count, instance_size_bytes) {}

PointCloud::PointCloud(VulkanEngine& engine, MeshManager& mesh_manager, MaterialInstanceManager& material_instance_manager, uint32_t instance_count) 
    :   InstancedRenderObject(engine, mesh_manager.point_cloud_quad, material_instance_manager.point_cloud, instance_count, sizeof(PointInstance))
{}

PointCloud::PointCloud(ManagerBundle& manager_bundle, uint32_t instance_count)
    :   PointCloud(manager_bundle.engine(), manager_bundle.mesh_manager(), manager_bundle.material_instance_manager(), instance_count) {}

PointCloud::PointCloud(ManagerBundle& manager_bundle, const std::vector<PointInstance>& points)
    :   PointCloud(manager_bundle.engine(), manager_bundle.mesh_manager(), manager_bundle.material_instance_manager(), points.size()) {
    set_points(points);
}

void PointCloud::set_points(const std::vector<PointInstance>& points) {
    LOG_METHOD();

    logger.check(!points.empty(), "Points vector was empty");
    instance_data.set_instance_count(points.size());

    instance_data.buffer().upload(points.data(), points.size() * sizeof(PointInstance));
}