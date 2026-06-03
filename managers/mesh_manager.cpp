#include "mesh_manager.h"

#include "../vulkan_self/vulkan_engine.h"
#include "../vulkan_self/vulkan_resource_loader.h"
#include "../renderer/static_mesh_data.h"

MeshManager::MeshManager(VulkanEngine& engine, VulkanResourceLoader& resource_loader)
    :   cube(engine, resource_loader, StaticMeshData::cube_vertices.data(), Utils::size_bytes(StaticMeshData::cube_vertices), 
                                       StaticMeshData::cube_indices.data(), Utils::size_bytes(StaticMeshData::cube_indices)),
        
        point_cloud_quad(engine, resource_loader, StaticMeshData::point_cloud_quad_corners.data(), Utils::size_bytes(StaticMeshData::point_cloud_quad_corners), 
                                       StaticMeshData::point_cloud_quad_indices.data(), Utils::size_bytes(StaticMeshData::point_cloud_quad_indices)){
}