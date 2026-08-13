#include "point_cloud.h"

#include "../../managers/material_instance_manager.h"
#include "../../managers/mesh_manager.h"
#include "point_instance.h"


PointCloud::PointCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material)
    :   InstancedRenderObject(engine, mesh, material) {}

PointCloud::PointCloud(VulkanEngine& engine, MeshManager& mesh_manager, MaterialInstanceManager& material_instance_manager)
    :   PointCloud(engine, mesh_manager.point_cloud_quad, material_instance_manager.point_cloud) {}

PointCloud::PointCloud(ManagerBundle& manager_bundle)
    :   PointCloud(manager_bundle.engine(), manager_bundle.mesh_manager(), 
                   manager_bundle.material_instance_manager()) {}

PointCloud::PointCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material, uint32_t instance_count)
    :   InstancedRenderObject(engine, mesh, material) {
    m_instance_batch = std::make_unique<InstanceBatch>(engine, instance_count, sizeof(PointInstance));
    set_instance_view(m_instance_batch->get_view());
}

PointCloud::PointCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material, InstanceBatch instance_batch)
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

PointCloud::PointCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material,
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

PointCloud::PointCloud(ManagerBundle& manager_bundle, std::filesystem::path path)
    :   PointCloud(load(manager_bundle, path)) {
}

void PointCloud::save(std::filesystem::path path) {
    LOG_METHOD();

    logger().check(!path.empty(), "Output path was empty");
    logger().check(path.has_filename(), "Output path has no filename");

    const auto parent = path.parent_path();

    logger().check(
        parent.empty() || std::filesystem::is_directory(parent),
        "Output folder does not exist: " + parent.string()
    );

    logger().check(
        m_instance_batch->instance_count() > 0,
        "Point cloud didn't have any points"
    );

    std::ofstream out(path, std::ios::binary | std::ios::trunc);

    logger().check(
        out.is_open(),
        "Failed to create file: " + path.string()
    );

    std::vector<PointInstance> points(m_instance_batch.get()->instance_count());

    m_instance_batch.get()->buffer().read(points.data(), sizeof(PointInstance) * points.size(), 0);

    uint64_t point_count = points.size();
    out.write(
        reinterpret_cast<const char*>(&point_count),
        static_cast<std::streamsize>(sizeof(uint64_t))
    );

    logger().check(
        out.good(),
        "Failed to write point count to file: " + path.string()
    );

    out.write(
        reinterpret_cast<const char*>(points.data()),
        static_cast<std::streamsize>(sizeof(PointInstance) * point_count)
    );

    logger().check(
        out.good(),
        "Failed to write points to file: " + path.string()
    );
}

PointCloud PointCloud::load(ManagerBundle& manager_bundle, std::filesystem::path path) {
    LOG_NAMED("PointCloud");
    
    logger().check(!path.empty(), "Output path was empty");
    logger().check(path.has_filename(), "Output path has no filename");

    const auto parent = path.parent_path();

    logger().check(
        parent.empty() || std::filesystem::is_directory(parent),
        "Output folder does not exist: " + parent.string()
    );

    std::ifstream file(path, std::ios::binary);

    logger().check(
        file.is_open(),
        "Failed to open file: " + path.string()
    );

    uint64_t point_count = 0;
    file.read(
        reinterpret_cast<char*>(&point_count),
        static_cast<std::streamsize>(sizeof(uint64_t))
    );

    logger().check(
        static_cast<bool>(file),
        "Failed to read point count from: " + path.string()
    );

    logger().check(
        point_count > 0,
        "Point count was 0"
    );

    logger().check(
        point_count <= std::numeric_limits<std::size_t>::max(),
        "Point count is too large"
    );
    
    std::vector<PointInstance> points(static_cast<std::size_t>(point_count));

    file.read(
        reinterpret_cast<char*>(points.data()),
        static_cast<std::streamsize>(sizeof(PointInstance) * point_count)
    );

    logger().check(
        static_cast<bool>(file),
        "Failed to read point data from: " + path.string()
    );
    
    return PointCloud(manager_bundle, points);
}

void PointCloud::set_points(const std::vector<PointInstance>& points) {
    LOG_METHOD();

    logger().check(!points.empty(), "Points vector was empty");

    // m_instance_batch->set_instance_count(points.size());
    // m_instance_batch->buffer().upload(points.data(), points.size() * sizeof(PointInstance));
    
    // set_instance_view(m_instance_batch->get_view());


    set_instance_count(points.size());
    instance_buffer().upload(points);
}

void PointCloud::set_color(glm::vec4 color) {
    set_material_data(PointCloudMaterialData{.color = color});
}

uint32_t PointCloud::point_count() const noexcept {
    return m_instance_batch->instance_count();
}
