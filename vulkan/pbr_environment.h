#pragma once
#include "texture.h"
#include "cubemap.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <functional>
#include <string>
#include <stdexcept>

class VulkanEngine;

class PBREnvironment {
public:
    using CubemapPass = std::function<void(
        Cubemap& target,
        uint32_t mip,
        uint32_t face,
        const glm::mat4& proj,
        const glm::mat4& view,
        float roughness
    )>;

    using Texture2DPass = std::function<void(
        Texture& target,
        uint32_t width,
        uint32_t height
    )>;

    Texture equirectangular;
    Cubemap environment;
    Cubemap irradiance;
    Cubemap prefilter;
    Texture* brdf_lut = nullptr;

    PBREnvironment() = default;

    PBREnvironment(
        VulkanEngine& engine,
        const std::string& filepath,
        Texture& brdf_lut,
        const CubemapPass& environment_pass,
        const CubemapPass& irradiance_pass,
        const CubemapPass& prefilter_pass,
        int size
    );

    Cubemap generate_environment_map(
        VulkanEngine& engine,
        const CubemapPass& render_pass,
        int size
    );

    Cubemap generate_irradiance_map(
        VulkanEngine& engine,
        const CubemapPass& render_pass
    );

    Cubemap generate_prefilter_map(
        VulkanEngine& engine,
        const CubemapPass& render_pass
    );

    static void generate_brdf_lut(
        VulkanEngine& engine,
        Texture& out_lut,
        const Texture2DPass& render_pass,
        int size = 512
    );

    static glm::mat4 capture_projection();
    static std::array<glm::mat4, 6> capture_views();
};