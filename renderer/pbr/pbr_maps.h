#pragma once
#include "../../vulkan_self/image/cubemap.h"

class VulkanTexture2D;

class PBRMaps {
public:
    PBRMaps(Cubemap&& environment_map, Cubemap&& irradiance_map, Cubemap&& prefilter_map, VulkanTexture2D& brdf_lut);

    Cubemap& environment_map() noexcept;
    Cubemap& irradiance_map() noexcept;
    Cubemap& prefilter_map() noexcept;
    VulkanTexture2D& brdf_lut() noexcept;

private:
    Cubemap m_environment_map;
    Cubemap m_irradiance_map;
    Cubemap m_prefilter_map;
    VulkanTexture2D& m_brdf_lut;
};