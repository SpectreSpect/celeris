#pragma once

#include "../renderer/mesh.h"


class VulkanEngine;
class VulkanResourceLoader;

class MeshManager {
public:
    Mesh cube;
    Mesh point_cloud_quad;
    
    MeshManager(VulkanEngine& engine, VulkanResourceLoader& resource_loader);
};