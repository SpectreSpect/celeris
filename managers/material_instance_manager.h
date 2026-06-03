#pragma once

#include "../renderer/material_data_types.h"

class VulkanEngine;
class MaterialManager;
class TextureManager;

class MaterialInstanceManager {
private:
    VulkanEngine& m_engine;
    MaterialManager& m_material_manager;
    TextureManager& m_texture_manager;

public:
    MaterialInstance unlit;
    MaterialInstance rock_blinn_phong;
    MaterialInstance dirt_blinn_phong;
    MaterialInstance point_cloud;

    VulkanBuffer point_cloud_ubo;


    MaterialInstanceManager(VulkanEngine& engine, MaterialManager& material_manager, TextureManager& texture_manager);

private:

    MaterialInstance create_mi(MaterialPass& material_pass, uint32_t material_data_size);
};

