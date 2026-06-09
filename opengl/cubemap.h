#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <array>
#include <string>
#include <iostream>
#include <cmath>
#include <algorithm>

class Cubemap {
public:
    enum class MinFilter {
        Nearest,
        Linear,
        NearestMipmapNearest,
        LinearMipmapNearest,
        NearestMipmapLinear,
        LinearMipmapLinear
    };

    enum class MagFilter {
        Nearest,
        Linear
    };

    GLuint id = 0;

    Cubemap() = default;

    // Order:
    // 0 = +X (right)
    // 1 = -X (left)
    // 2 = +Y (top)
    // 3 = -Y (bottom)
    // 4 = +Z
    // 5 = -Z
    Cubemap(const std::array<std::string, 6>& facepaths,
            MagFilter mag_filter = MagFilter::Linear,
            MinFilter min_filter = MinFilter::Linear,
            bool srgb = false,
            bool flipY = false);

    ~Cubemap();

    Cubemap(const Cubemap&) = delete;
    Cubemap& operator=(const Cubemap&) = delete;

    Cubemap(Cubemap&& other) noexcept;
    Cubemap& operator=(Cubemap&& other) noexcept;

    bool loadFromFaces(const std::array<std::string, 6>& facepaths,
                       MagFilter mag_filter = MagFilter::Linear,
                       MinFilter min_filter = MinFilter::Linear,
                       bool srgb = false,
                       bool flipY = false);

    // Creates an empty cubemap you can render into.
    // Example internal formats:
    // GL_RGB8, GL_RGBA8, GL_RGB16F, GL_RGBA16F, GL_DEPTH_COMPONENT24, ...
    void createEmpty(int size,
                     GLenum internal_format = GL_RGB16F,
                     MagFilter mag_filter = MagFilter::Linear,
                     MinFilter min_filter = MinFilter::Linear);

    void bind(int slot) const;

    void destroy();

    int faceSize() const { return m_size; }
    int mipLevels() const { return m_levels; }

private:
    int m_size = 0;
    int m_levels = 1;

    static int compute_mip_levels(int size);

    static GLenum toGLMagFilter(MagFilter f);
    static std::pair<GLenum, bool> toGLMinFilter(MinFilter f);
};