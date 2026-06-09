#include "pbr_maps.h"

PBRMaps::PBRMaps(Cubemap&& environment_map, Cubemap&& irradiance_map, Cubemap&& prefilter_map, VulkanTexture2D& brdf_lut) 
    :   m_environment_map(std::move(environment_map)),
        m_irradiance_map(std::move(irradiance_map)),
        m_prefilter_map(std::move(prefilter_map)),
        m_brdf_lut(brdf_lut){}

Cubemap& PBRMaps::environment_map() noexcept {
    return m_environment_map;
}

Cubemap& PBRMaps::irradiance_map() noexcept {
    return m_irradiance_map;
}

Cubemap& PBRMaps::prefilter_map() noexcept {
    return m_prefilter_map;
}

VulkanTexture2D& PBRMaps::brdf_lut() noexcept {
    return m_brdf_lut;
}