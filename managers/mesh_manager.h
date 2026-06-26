#pragma once

#include "../renderer/mesh.h"


class VulkanEngine;
class VulkanResourceLoader;

class MeshManager {
public:
    Mesh cube;
    Mesh sphere;
    Mesh point_cloud_quad;
    Mesh skybox_cube;

    Mesh two_sphere_indirect_test;

    
    MeshManager(VulkanEngine& engine, VulkanResourceLoader& resource_loader);
};
