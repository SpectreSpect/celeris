#include "pbr_map_generator.h"

#include "../../vulkan_self/image/vulkan_texture_2d.h"
#include "pbr_maps.h"

PBRMapGenerator::PBRMapGenerator(VulkanEngine& engine, ComputePassManager& compute_pass_manager)
    :   m_equirect_to_cubemap_pass(engine, compute_pass_manager),
        m_brdf_lut_pass(engine, compute_pass_manager),
        m_prefilter_pass(engine, compute_pass_manager),
        m_irradiance_pass(engine, compute_pass_manager),
        m_brdf_lut(m_brdf_lut_pass.generate(512, 512)) {}

PBRMaps PBRMapGenerator::generate_maps(VulkanTexture2D& equirect_map, 
                                       uint32_t environment_map_size, 
                                       uint32_t prefilter_map_size, 
                                       uint32_t irradiance_map_size) {
    if (environment_map_size == std::numeric_limits<uint32_t>::max()) {
        VkExtent2D env_map_extent = equirect_map.extent2d();
        environment_map_size = std::max(env_map_extent.width, env_map_extent.height);
    }
    
    Cubemap environemnt_map(m_equirect_to_cubemap_pass.generate(equirect_map, environment_map_size));
    Cubemap prefilter_map(m_prefilter_pass.generate(environemnt_map, prefilter_map_size));
    Cubemap irradiance_map(m_irradiance_pass.generate(environemnt_map, irradiance_map_size));

    return PBRMaps(std::move(environemnt_map), std::move(irradiance_map), std::move(prefilter_map), m_brdf_lut);
}