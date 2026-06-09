#pragma once
#include "texture.h"
#include "cubemap.h"
#include "mesh.h"
#include "framebuffer.h"

class Engine3D;

class PBREnvironment {
public:
    Texture equirectangular;
    Cubemap environment;
    Cubemap irradiance;
    Cubemap prefilter;
    Texture brdf_lut;

    PBREnvironment() = default;
    PBREnvironment(Engine3D& engine, const std::string& filepath, Texture& brdf_lut, Framebuffer& capture_fbo, Mesh* cube_mesh, int size);


    Cubemap generate_environment_map(Engine3D& engine, glm::mat4& captureProj, 
                                     Texture& hdrEquirect, Framebuffer& capture_fbo, std::array<glm::mat4, 6> captureViews, Mesh* cube_mesh, int size);
    Cubemap generate_irradiance_map(Engine3D& engine, const Cubemap& environment_map, 
                                    const Mesh* cube_mesh, std::array<glm::mat4, 6> captureViews, glm::mat4 captureProj);
    Cubemap generate_prefilter_map(Engine3D& engine, const Cubemap& environment_map, 
                                   const Mesh* cube_mesh, std::array<glm::mat4, 6> captureViews, glm::mat4 captureProj);
    static Texture generate_brdf_lut(Engine3D& engine, Mesh* quad_mesh);
};