#pragma once
#include "../vulkan_engine.h"
#include "pipeline.h"

class Mesh;

class Renderer {
public:
    // Renderer(VulkanEngine& engine, Pipeline& pipeline);
    Renderer() = default;
    // virtual void render(Drawable& drawable, Camera& camera) = 0;
    virtual void render_mesh(Mesh& mesh, RenderState state) = 0;
};