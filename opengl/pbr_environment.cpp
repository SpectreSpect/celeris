#include "pbr_environment.h"
#include "engine3d.h"


PBREnvironment::PBREnvironment(Engine3D& engine, const std::string& filepath, Texture& brdf_lut, Framebuffer& capture_fbo, Mesh* cube_mesh, int size) {
    this->brdf_lut = brdf_lut;


    static glm::mat4 captureProj = glm::perspective(
        glm::radians(90.0f), 1.0f, 0.1f, 10.0f
    );

    static std::array<glm::mat4, 6> captureViews = {
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // +X
        glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // -X
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)), // +Y
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)), // -Y
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // +Z
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))  // -Z
    };

    equirectangular.loadFromFile(
            filepath,
            Texture::Wrap::ClampToEdge,
            Texture::MagFilter::Linear,
            Texture::MinFilter::Linear,
            false,
            true);
    
    environment = generate_environment_map(engine, captureProj, equirectangular, capture_fbo, captureViews, cube_mesh, size);
    irradiance = generate_irradiance_map(engine, environment, cube_mesh, captureViews, captureProj);
    prefilter = generate_prefilter_map(engine, environment, cube_mesh, captureViews, captureProj);
}

Cubemap PBREnvironment::generate_environment_map(Engine3D& engine, glm::mat4& captureProj, 
                                              Texture& hdrEquirect, Framebuffer& capture_fbo, 
                                              std::array<glm::mat4, 6> captureViews, Mesh* cube_mesh, int size) {
    Cubemap envMap;
    envMap.createEmpty(
        size,
        GL_RGB16F,
        Cubemap::MagFilter::Linear,
        Cubemap::MinFilter::LinearMipmapLinear
    );

    engine.equirect_to_cubemap_program->use();
    engine.equirect_to_cubemap_program->set_mat4("uProj", captureProj);
    engine.equirect_to_cubemap_program->set_int("uEquirectangularMap", 0);
    hdrEquirect.bind(0);

    capture_fbo.bind();
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    for (int face = 0; face < 6; ++face) {
        capture_fbo.attachCubemapFace(envMap.id, face, GL_COLOR_ATTACHMENT0, 0);

        engine.equirect_to_cubemap_program->use();
        engine.equirect_to_cubemap_program->set_mat4("uView", captureViews[face]);

        glViewport(0, 0, size, size);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        cube_mesh->vao->bind();
        glDrawElements(GL_TRIANGLES, cube_mesh->ebo->num_indices, GL_UNSIGNED_INT, 0);
        cube_mesh->vao->unbind();
    }

    Framebuffer::unbind();
    glGenerateTextureMipmap(envMap.id);

    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);

    return envMap;
}

Cubemap PBREnvironment::generate_irradiance_map(Engine3D& engine, const Cubemap& environment_map, 
                                                const Mesh* cube_mesh, std::array<glm::mat4, 6> captureViews, glm::mat4 captureProj) {
    Cubemap irradianceMap;
    int irradianceSize = 32;

    irradianceMap.createEmpty(
        irradianceSize,
        GL_RGB16F,
        Cubemap::MagFilter::Linear,
        Cubemap::MinFilter::Linear
    );

    Framebuffer irradianceFBO(irradianceSize, irradianceSize);
    irradianceFBO.attachCubemapFace(irradianceMap.id, 0, GL_COLOR_ATTACHMENT0, 0);
    irradianceFBO.createDepthRenderbuffer(GL_DEPTH_COMPONENT24);
    irradianceFBO.setDrawBuffer(GL_COLOR_ATTACHMENT0);

    if (!irradianceFBO.isComplete()) {
        std::cerr << "Irradiance FBO incomplete\n";
    }

    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    irradianceFBO.bind();
    glViewport(0, 0, irradianceSize, irradianceSize);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    engine.irradiance_program->use();
    engine.irradiance_program->set_mat4("uProj", captureProj);
    engine.irradiance_program->set_int("uEnvironmentMap", 0);
    environment_map.bind(0);

    for (int face = 0; face < 6; ++face) {
        irradianceFBO.attachCubemapFace(irradianceMap.id, face, GL_COLOR_ATTACHMENT0, 0);
        engine.irradiance_program->set_mat4("uView", captureViews[face]);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        cube_mesh->vao->bind();
        glDrawElements(GL_TRIANGLES, cube_mesh->ebo->num_indices, GL_UNSIGNED_INT, 0);
        cube_mesh->vao->unbind();
    }

    Framebuffer::unbind();
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);


    return irradianceMap;
}

Cubemap PBREnvironment::generate_prefilter_map(Engine3D& engine, const Cubemap& environment_map, 
                                               const Mesh* cube_mesh, std::array<glm::mat4, 6> captureViews, glm::mat4 captureProj) {
    Cubemap prefilterMap;
    int prefilterSize = 2048;
    // int prefilterSize = 256;

    prefilterMap.createEmpty(
        prefilterSize,
        GL_RGB16F,
        Cubemap::MagFilter::Linear,
        Cubemap::MinFilter::LinearMipmapLinear
    );

    Framebuffer prefilterFBO(prefilterSize, prefilterSize);
    prefilterFBO.attachCubemapFace(prefilterMap.id, 0, GL_COLOR_ATTACHMENT0, 0);
    prefilterFBO.createDepthRenderbuffer(GL_DEPTH_COMPONENT24);
    prefilterFBO.setDrawBuffer(GL_COLOR_ATTACHMENT0);

    if (!prefilterFBO.isComplete()) {
        std::cerr << "Prefilter FBO incomplete\n";
    }

    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    prefilterFBO.bind();
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    engine.prefilter_program->use();
    engine.prefilter_program->set_mat4("uProj", captureProj);
    engine.prefilter_program->set_int("uEnvironmentMap", 0);
    engine.prefilter_program->set_float("uEnvironmentResolution", float(environment_map.faceSize()));
    environment_map.bind(0);

    int maxMipLevels = prefilterMap.mipLevels();

    for (int mip = 0; mip < maxMipLevels; ++mip) {
        int mipWidth  = std::max(1, prefilterSize >> mip);
        int mipHeight = std::max(1, prefilterSize >> mip);

        glViewport(0, 0, mipWidth, mipHeight);

        // Depth RBO must match the current mip size
        glNamedRenderbufferStorage(
            prefilterFBO.depth_rbo,
            GL_DEPTH_COMPONENT24,
            mipWidth,
            mipHeight
        );

        float roughness = (maxMipLevels == 1)
            ? 0.0f
            : float(mip) / float(maxMipLevels - 1);

        engine.prefilter_program->use();
        engine.prefilter_program->set_float("uRoughness", roughness);

        for (int face = 0; face < 6; ++face) {
            prefilterFBO.attachCubemapFace(prefilterMap.id, face, GL_COLOR_ATTACHMENT0, mip);
            engine.prefilter_program->set_mat4("uView", captureViews[face]);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            cube_mesh->vao->bind();
            glDrawElements(GL_TRIANGLES, cube_mesh->ebo->num_indices, GL_UNSIGNED_INT, 0);
            cube_mesh->vao->unbind();
        }
    }

    Framebuffer::unbind();
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);

    return prefilterMap;
}


Texture PBREnvironment::generate_brdf_lut(Engine3D& engine, Mesh* quad_mesh) {
    Texture brdfLUT;

    glCreateTextures(GL_TEXTURE_2D, 1, &brdfLUT.id);
    glTextureStorage2D(brdfLUT.id, 1, GL_RGBA16F, 512, 512);
    glTextureParameteri(brdfLUT.id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(brdfLUT.id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(brdfLUT.id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(brdfLUT.id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    Framebuffer brdfFBO(512, 512);
    brdfFBO.attachTexture2D(brdfLUT.id, GL_COLOR_ATTACHMENT0, 0);
    brdfFBO.setDrawBuffer(GL_COLOR_ATTACHMENT0);

    if (!brdfFBO.isComplete()) {
        std::cerr << "BRDF LUT FBO incomplete\n";
    }

    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    GLboolean wasBlendEnabled = glIsEnabled(GL_BLEND);
    GLboolean wasCullEnabled  = glIsEnabled(GL_CULL_FACE);
    GLboolean wasDepthEnabled = glIsEnabled(GL_DEPTH_TEST);

    brdfFBO.bind();
    glViewport(0, 0, 512, 512);

    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    engine.brdf_lut_program->use();

    glClear(GL_COLOR_BUFFER_BIT);

    quad_mesh->vao->bind();
    glDrawElements(GL_TRIANGLES, quad_mesh->ebo->num_indices, GL_UNSIGNED_INT, 0);
    quad_mesh->vao->unbind();

    Framebuffer::unbind();
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);

    if (wasBlendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (wasCullEnabled)  glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (wasDepthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);

    return brdfLUT;
}