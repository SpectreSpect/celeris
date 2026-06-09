#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "vulkan_engine.h"
#include "drawable.h"
#include "transformable.h"
#include "vulkan/video_buffer.h"
#include "vulkan/vulkan_vertex_layout.h"

class Mesh : public Drawable, public Transformable {
public:
    VideoBuffer vertex_buffer;
    VideoBuffer index_buffer;
    uint32_t num_indices = 0;
    
    Mesh() = default;
    Mesh(
        VulkanEngine* engine,
        std::vector<std::byte> vertex_data,
        std::vector<uint32_t> index_data,
        VideoBuffer& staging_buffer
    );

    void draw(RenderState state) override;
};