#include "pbr_environment.h"
#include "../vulkan_engine.h"

PBREnvironment::PBREnvironment(
    VulkanEngine& engine,
    const std::string& filepath,
    Texture& brdf_lut,
    const CubemapPass& environment_pass,
    const CubemapPass& irradiance_pass,
    const CubemapPass& prefilter_pass,
    int size
) {
    this->brdf_lut = &brdf_lut;

    if (!equirectangular.loadFromFile(
            engine,
            filepath,
            Texture::Wrap::ClampToEdge,
            Texture::MagFilter::Linear,
            Texture::MinFilter::Linear,
            false,
            true))
    {
        throw std::runtime_error("Failed to load equirectangular environment: " + filepath);
    }

    environment = generate_environment_map(engine, environment_pass, size);
    irradiance = generate_irradiance_map(engine, irradiance_pass);
    prefilter = generate_prefilter_map(engine, prefilter_pass);
}

glm::mat4 PBREnvironment::capture_projection() {
    return glm::perspective(
        glm::radians(90.0f),
        1.0f,
        0.1f,
        10.0f
    );
}

std::array<glm::mat4, 6> PBREnvironment::capture_views() {
    return {
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // +X
        glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // -X
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)), // +Y
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)), // -Y
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // +Z
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))  // -Z
    };
}

Cubemap PBREnvironment::generate_environment_map(
    VulkanEngine& engine,
    const CubemapPass& render_pass,
    int size
) {
    if (!render_pass) {
        throw std::runtime_error("generate_environment_map: render_pass callback was empty");
    }

    Cubemap env_map;
    env_map.createEmpty(
        engine,
        size,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        Cubemap::MagFilter::Linear,
        Cubemap::MinFilter::LinearMipmapLinear
    );

    const glm::mat4 proj = capture_projection();
    const auto views = capture_views();

    for (uint32_t face = 0; face < 6; ++face) {
        render_pass(env_map, 0, face, proj, views[face], 0.0f);
    }

    return env_map;
}

Cubemap PBREnvironment::generate_irradiance_map(
    VulkanEngine& engine,
    const CubemapPass& render_pass
) {
    if (!render_pass) {
        throw std::runtime_error("generate_irradiance_map: render_pass callback was empty");
    }

    Cubemap irradiance_map;
    const int irradiance_size = 32;

    irradiance_map.createEmpty(
        engine,
        irradiance_size,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        Cubemap::MagFilter::Linear,
        Cubemap::MinFilter::Linear
    );

    const glm::mat4 proj = capture_projection();
    const auto views = capture_views();

    for (uint32_t face = 0; face < 6; ++face) {
        render_pass(irradiance_map, 0, face, proj, views[face], 0.0f);
    }

    return irradiance_map;
}

Cubemap PBREnvironment::generate_prefilter_map(
    VulkanEngine& engine,
    const CubemapPass& render_pass
) {
    if (!render_pass) {
        throw std::runtime_error("generate_prefilter_map: render_pass callback was empty");
    }

    Cubemap prefilter_map;
    const int prefilter_size = 2048;
    // const int prefilter_size = 256;

    prefilter_map.createEmpty(
        engine,
        prefilter_size,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        Cubemap::MagFilter::Linear,
        Cubemap::MinFilter::LinearMipmapLinear
    );

    const glm::mat4 proj = capture_projection();
    const auto views = capture_views();
    const int max_mip_levels = prefilter_map.mipLevels();

    for (int mip = 0; mip < max_mip_levels; ++mip) {
        const float roughness = (max_mip_levels == 1)
            ? 0.0f
            : float(mip) / float(max_mip_levels - 1);

        for (uint32_t face = 0; face < 6; ++face) {
            render_pass(prefilter_map, static_cast<uint32_t>(mip), face, proj, views[face], roughness);
        }
    }

    return prefilter_map;
}

void PBREnvironment::generate_brdf_lut(
    VulkanEngine& engine,
    Texture& out_lut,
    const Texture2DPass& render_pass,
    int size
) {
    if (!render_pass) {
        throw std::runtime_error("generate_brdf_lut: render_pass callback was empty");
    }

    out_lut.create(
        engine,
        size,
        size,
        nullptr,
        Texture::Wrap::ClampToEdge,
        Texture::MagFilter::Linear,
        Texture::MinFilter::Linear,
        false
    );

    render_pass(out_lut, static_cast<uint32_t>(size), static_cast<uint32_t>(size));
}