#include "material_instance_manager.h"
#include "../vulkan_self/vulkan_engine.h"
#include "material_manager.h"
#include "texture_manager.h"

MaterialInstanceManager::MaterialInstanceManager(VulkanEngine& engine, MaterialManager& material_manager, TextureManager& texture_manager)
    :   m_engine(engine),
        m_material_manager(material_manager),
        m_texture_manager(texture_manager),
        unlit(create_mi(material_manager.unlit_mp, sizeof(UnlitMaterialData))),
        rock_blinn_phong(material_manager.create_blinn_phong_material(engine, texture_manager.rock_texture)),
        dirt_blinn_phong(material_manager.create_blinn_phong_material(engine, texture_manager.dirt_texture)),
        point_cloud(create_mi(material_manager.point_mp, sizeof(UnlitMaterialData))),
        st_peters_square_night_4k_hdr(material_manager.create_skybox_material(engine, texture_manager.hdr_env_maps[TextureManager::st_peters_square_night_4k_pbr_map_id])),
        dirt_pbr(material_manager.create_pbr_material(engine, texture_manager.irradiance_maps, texture_manager.prefilter_maps, texture_manager.brdf_lut)),
        pbr(material_manager.create_pbr_material(engine, texture_manager.irradiance_maps, texture_manager.prefilter_maps, texture_manager.brdf_lut)),
        voxel_mesh(create_mi(material_manager.voxel_mesh_mp, sizeof(UnlitMaterialData))),
        voxel_pbr(material_manager.create_voxel_pbr_material(engine, texture_manager.irradiance_maps, texture_manager.prefilter_maps, texture_manager.brdf_lut)),
        point_cloud_ubo(VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(PointUniform))){

    PointUniform point_uniform{2, 0.005, 1, 0};
    point_cloud_ubo.upload(&point_uniform, sizeof(point_uniform));
    point_cloud.set_uniform_buffer(1, point_cloud_ubo);
}

SlotPassInstance MaterialInstanceManager::create_mi(MaterialPass& material_pass, uint32_t material_data_size) {
    return SlotPassInstance(m_engine, m_material_manager.descriptor_pool(), material_pass, material_data_size);
}
