#pragma once

#include <vector>

#include "../instanced_render_object.h"
#include "../manager_bundle.h"
#include "../../vulkan_self/logger/logger.h"

class VulkanEngine;
class MeshManager;
class MaterialInstanceManager;
struct PointInstance;

class PointCloud : public InstancedRenderObject {
public:
    _XCHILD_NAME(PointCloud);

    PointCloud(VulkanEngine& engine, Mesh& mesh, MaterialInstance& material, uint32_t instance_count, uint32_t instance_size_bytes);
    PointCloud(VulkanEngine& engine, MeshManager& mesh_manager, MaterialInstanceManager& material_instance_manager, uint32_t instance_count);
    PointCloud(ManagerBundle& manager_bundle, uint32_t instance_count);
    PointCloud(ManagerBundle& manager_bundle, const std::vector<PointInstance>& points);

    void set_points(const std::vector<PointInstance>& points);
};
