#pragma once

#include "material_data_types.h"
#include "../vulkan_self/material/material_instance.h"

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
    MaterialInstance st_peters_square_night_4k_hdr;
    MaterialInstance dirt_pbr;
    MaterialInstance studio_kominka_02_4k_pbr;

    

    VulkanBuffer point_cloud_ubo;


    MaterialInstanceManager(VulkanEngine& engine, MaterialManager& material_manager, TextureManager& texture_manager);

private:

    MaterialInstance create_mi(MaterialPass& material_pass, uint32_t material_data_size);
};

