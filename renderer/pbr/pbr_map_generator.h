#pragma once

#include <limits>

#include "../../vulkan_self/image/vulkan_texture_2d.h"
#include "equirect_to_cubemap_pass.h"
#include "brdf_lut_pass.h"
#include "prefilter_map_pass.h"
#include "irradiance_map_pass.h"

class PBRMaps;

class PBRMapGenerator {
public:
    PBRMapGenerator(VulkanEngine& engine, ComputePassManager& compute_pass_manager);

    PBRMaps generate_maps(VulkanTexture2D& equirect_map, 
                          uint32_t environment_map_size = std::numeric_limits<uint32_t>::max(), 
                          uint32_t prefilter_map_size = 512, 
                          uint32_t irradiance_map_size = 32);

private:
    EquirectToCubemapPass m_equirect_to_cubemap_pass;
    BrdfLutPass m_brdf_lut_pass;
    PrefilterPass m_prefilter_pass;
    IrradiancePass m_irradiance_pass;

    VulkanTexture2D m_brdf_lut;
};