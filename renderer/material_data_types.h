#pragma once

#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

struct UnlitMaterialData {
    glm::vec4 color;
};

struct BlinPhongMaterialData {
    glm::vec4 material;
    glm::vec4 color;
};

struct SkyboxMaterialData {
    float exposure;
};

struct PBRMaterialData {
    glm::vec4 material = glm::vec4(1.0f, 0.01f, 1.0f, 0.2f);
    glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    glm::uvec4 pbr_map_ids = glm::uvec4(0u, 0u, 0u, 0u);

    static PBRMaterialData create(
        float metallic = 1.0f,
        float roughness = 0.01f,
        float exposure = 1.0f,
        glm::vec4 base_color = glm::vec4(1.0f),
        float ambient_occlusion = 1.0f
    ) {
        PBRMaterialData data{};
        data.material = glm::vec4(metallic, roughness, ambient_occlusion, exposure);
        data.color = base_color;
        return data;
    }

    static PBRMaterialData with_pbr_maps(
        uint32_t pbr_map_id,
        float metallic = 1.0f,
        float roughness = 0.01f,
        float exposure = 1.0f,
        glm::vec4 base_color = glm::vec4(1.0f),
        float ambient_occlusion = 1.0f
    ) {
        PBRMaterialData data = create(
            metallic,
            roughness,
            exposure,
            base_color,
            ambient_occlusion
        );
        data.set_pbr_map_id(pbr_map_id);
        return data;
    }

    void set_pbr_map_id(uint32_t pbr_map_id) noexcept {
        pbr_map_ids.x = pbr_map_id;
        pbr_map_ids.y = pbr_map_id;
    }
};

struct PointUniform {
    float point_size_px;
    float point_size_world;
    int screen_space_size;
    float pad;
};
