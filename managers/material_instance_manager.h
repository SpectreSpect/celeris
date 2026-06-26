#pragma once

#include "../renderer/material_data_types.h"
#include "../vulkan_self/pass/instance/slot_pass_instance.h"
#include "../vulkan_self/vulkan_buffer.h"

class VulkanEngine;
class MaterialManager;
class TextureManager;
class MaterialPass;

class MaterialInstanceManager {
private:
    VulkanEngine& m_engine;
    MaterialManager& m_material_manager;
    TextureManager& m_texture_manager;

public:
    SlotPassInstance unlit;
    SlotPassInstance rock_blinn_phong;
    SlotPassInstance dirt_blinn_phong;
    SlotPassInstance point_cloud;
    SlotPassInstance st_peters_square_night_4k_hdr;
    SlotPassInstance dirt_pbr;
    SlotPassInstance pbr;
    SlotPassInstance voxel_mesh;
    SlotPassInstance voxel_pbr;

    VulkanBuffer point_cloud_ubo;


    MaterialInstanceManager(VulkanEngine& engine, MaterialManager& material_manager, TextureManager& texture_manager);

private:

    SlotPassInstance create_mi(MaterialPass& material_pass, uint32_t material_data_size);
};
