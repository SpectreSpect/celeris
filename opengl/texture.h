#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>



class Texture {
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

    enum class Wrap {
        ClampToEdge,
        Repeat,
        MirroredRepeat
    };

    GLuint id = 0;    

    Texture() = default;
    Texture(int width, int height, const void* initial_data = nullptr);
    Texture(const std::string& filepath, 
            Wrap wrap = Wrap::Repeat,
            MagFilter mag_filter = MagFilter::Linear, 
            MinFilter min_filter = MinFilter::LinearMipmapLinear, 
            bool srgb = false, bool flipY = false);
    int compute_mip_levels(int w, int h);
    bool loadFromFile(const std::string& filepath, 
                      Wrap wrap = Wrap::Repeat,
                      MagFilter mag_filter = MagFilter::Linear, 
                      MinFilter min_filter = MinFilter::LinearMipmapLinear, 
                      bool srgb = false, bool flipY = false);
    void create(int width, int height, const void* initial_data = nullptr);
    void bind(int slot);
};