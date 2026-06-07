#include "mesh_manager.h"

#include "../vulkan_self/vulkan_engine.h"
#include "../vulkan_self/vulkan_resource_loader.h"
#include "../renderer/static_mesh_data.h"

MeshManager::MeshManager(VulkanEngine& engine, VulkanResourceLoader& resource_loader)
    :   sphere(engine, resource_loader, StaticMeshData::sphere_vertices.data(), Utils::size_bytes(StaticMeshData::sphere_vertices), 
                                        StaticMeshData::sphere_indices.data(), Utils::size_bytes(StaticMeshData::sphere_indices)),
        cube(engine, resource_loader, StaticMeshData::cube_vertices.data(), Utils::size_bytes(StaticMeshData::cube_vertices), 
                                       StaticMeshData::cube_indices.data(), Utils::size_bytes(StaticMeshData::cube_indices)),
        point_cloud_quad(engine, resource_loader, StaticMeshData::point_cloud_quad_corners.data(), Utils::size_bytes(StaticMeshData::point_cloud_quad_corners), 
                                       StaticMeshData::point_cloud_quad_indices.data(), Utils::size_bytes(StaticMeshData::point_cloud_quad_indices)),
        skybox_cube(engine, resource_loader, StaticMeshData::skybox_cube_vertices.data(), Utils::size_bytes(StaticMeshData::skybox_cube_vertices), 
                                             StaticMeshData::skybox_cube_indices.data(), Utils::size_bytes(StaticMeshData::skybox_cube_indices)),
        two_sphere_indirect_test(engine, resource_loader, StaticMeshData::two_sphere_indirect_test_vertices.data(), Utils::size_bytes(StaticMeshData::two_sphere_indirect_test_vertices), 
                                             StaticMeshData::two_sphere_indirect_test_indices.data(), Utils::size_bytes(StaticMeshData::two_sphere_indirect_test_indices)){
}