#pragma once

#include "mesh.h"


class VulkanEngine;
class VulkanResourceLoader;

class MeshManager {
public:
    Mesh cube;
    Mesh point_cloud_quad;
    Mesh skybox_cube;
    
    MeshManager(VulkanEngine& engine, VulkanResourceLoader& resource_loader);
};