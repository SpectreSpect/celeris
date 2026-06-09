#define STB_IMAGE_IMPLEMENTATION
#include "texture.h"
#include "stb_image.h"

Texture::Texture(int width, int height, const void* initial_data) {
    create(width, height, initial_data);
}

Texture::Texture(const std::string& filepath, 
                 Wrap wrap, MagFilter mag_filter, MinFilter min_filter,
                 bool srgb, bool flipY) {
    loadFromFile(filepath, wrap, mag_filter, min_filter, srgb, flipY);
}

void Texture::create(int width, int height, const void* initial_data) {
    glCreateTextures(GL_TEXTURE_2D, 1, &id);                // create object
    glTextureStorage2D(id, 1, GL_RGBA8, width, height);    // immutable storage (1 mip level)

    // upload subimage (level 0)
    glTextureSubImage2D(id, 0, 0,0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, initial_data);

    // parameters with DSA
    // glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    // glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenerateTextureMipmap(id); // DSA version of glGenerateMipmap
}

int Texture::compute_mip_levels(int w, int h) {
    return 1 + static_cast<int>(std::floor(std::log2(static_cast<float>(std::max(w, h)))));
}

bool Texture::loadFromFile(const std::string& filepath,
                           Wrap wrap, MagFilter mag_filter, MinFilter min_filter,
                           bool srgb, bool flipY) {
    stbi_set_flip_vertically_on_load(flipY ? 1 : 0);

    auto toGLWrap = [](Wrap w) -> GLenum {
        switch (w) {
            case Wrap::ClampToEdge:   return GL_CLAMP_TO_EDGE;
            case Wrap::Repeat:        return GL_REPEAT;
            case Wrap::MirroredRepeat: return GL_MIRRORED_REPEAT;
            default:                  return GL_CLAMP_TO_EDGE;
        }
    };

    // Returns a pair: <gl_enum_for_min_filter, needs_mipmaps>
    auto toGLMinFilter = [](MinFilter f) -> std::pair<GLenum,bool> {
        switch (f) {
            case MinFilter::Nearest:                  return { GL_NEAREST, false };
            case MinFilter::Linear:                   return { GL_LINEAR, false };
            case MinFilter::NearestMipmapNearest:     return { GL_NEAREST_MIPMAP_NEAREST, true };
            case MinFilter::LinearMipmapNearest:      return { GL_LINEAR_MIPMAP_NEAREST, true };
            case MinFilter::NearestMipmapLinear:      return { GL_NEAREST_MIPMAP_LINEAR, true };
            case MinFilter::LinearMipmapLinear:       return { GL_LINEAR_MIPMAP_LINEAR, true };
            default:                                  return { GL_LINEAR, false };
        }
    };

    auto toGLMagFilter = [](MagFilter f) -> GLenum {
        switch (f) {
            case MagFilter::Nearest: return GL_NEAREST;
            case MagFilter::Linear:  return GL_LINEAR;
            default:                 return GL_LINEAR;
        }
    };

    const GLenum wrapGL = toGLWrap(wrap);
    const auto[minFilterGL, minNeedsMips] = toGLMinFilter(min_filter);
    const GLenum magFilterGL = toGLMagFilter(mag_filter);

    // Try HDR first (float)
    int w=0, h=0, channels=0;
    float* floatData = stbi_loadf(filepath.c_str(), &w, &h, &channels, 0);
    if (floatData) {
        GLenum format = (channels == 3) ? GL_RGB : GL_RGBA;
        GLenum internal = (channels == 3) ? GL_RGB16F : GL_RGBA16F;

        // allocate either one level or full mip chain depending on chosen min filter
        int levels = minNeedsMips ? compute_mip_levels(w,h) : 1;

        glCreateTextures(GL_TEXTURE_2D, 1, &id);
        glTextureStorage2D(id, levels, internal, w, h);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTextureSubImage2D(id, 0, 0, 0, w, h, format, GL_FLOAT, floatData);

        if (minNeedsMips && levels > 1) {
            glGenerateTextureMipmap(id);
        }

        stbi_image_free(floatData);

        // set wrap & filters
        glTextureParameteri(id, GL_TEXTURE_WRAP_S, wrapGL);
        glTextureParameteri(id, GL_TEXTURE_WRAP_T, wrapGL);

        // If the user requested a mipmapped min filter but for some reason
        // we only allocated level 0 (shouldn't happen with the logic above),
        // the GL min filter would fall back. We allocated correctly above.
        glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, minFilterGL);
        glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, magFilterGL);

        return true;
    }

    // Not HDR: try 8-bit
    unsigned char* data = stbi_load(filepath.c_str(), &w, &h, &channels, 0);
    if (!data) {
        std::cerr << "stb_image failed to load: " << filepath << "\n";
        return false;
    }

    GLenum format = GL_RGBA;
    GLenum internal = GL_RGBA8;
    GLenum type = GL_UNSIGNED_BYTE;

    if (channels == 1) {
        format = GL_RED; internal = GL_R8;
    } else if (channels == 3) {
        format = GL_RGB;
        internal = srgb ? GL_SRGB8 : GL_RGB8;
    } else if (channels == 4) {
        format = GL_RGBA;
        internal = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    } else {
        std::cerr << "Unexpected number of channels (" << channels << "), forcing RGBA\n";
        format = GL_RGBA;
        internal = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    }

    // choose number of mip levels depending on requested min filter
    int levels = minNeedsMips ? compute_mip_levels(w,h) : 1;

    glCreateTextures(GL_TEXTURE_2D, 1, &id);
    glTextureStorage2D(id, levels, internal, w, h);

    GLint unpackAlignment = (channels == 3) ? 1 : 4;
    glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);

    glTextureSubImage2D(id, 0, 0, 0, w, h, format, type, data);

    if (minNeedsMips && levels > 1) {
        glGenerateTextureMipmap(id);
    }

    // set wrap & filters
    glTextureParameteri(id, GL_TEXTURE_WRAP_S, wrapGL);
    glTextureParameteri(id, GL_TEXTURE_WRAP_T, wrapGL);
    glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, minFilterGL);
    glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, magFilterGL);

    // optional anisotropic filtering
    GLfloat maxAniso = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
    if (maxAniso > 0.0f) {
        GLfloat aniso = std::min(4.0f, maxAniso);
        glTextureParameterf(id, GL_TEXTURE_MAX_ANISOTROPY, aniso);
    }

    stbi_image_free(data);
    return true;
}

void Texture::bind(int slot) {
    glBindTextureUnit(slot, id);
}