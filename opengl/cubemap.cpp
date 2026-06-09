#include "cubemap.h"
#include "stb_image.h"

Cubemap::Cubemap(const std::array<std::string, 6>& facepaths,
                 MagFilter mag_filter,
                 MinFilter min_filter,
                 bool srgb,
                 bool flipY) {
    loadFromFaces(facepaths, mag_filter, min_filter, srgb, flipY);
}

Cubemap::~Cubemap() {
    destroy();
}

Cubemap::Cubemap(Cubemap&& other) noexcept {
    id = other.id;
    m_size = other.m_size;
    m_levels = other.m_levels;

    other.id = 0;
    other.m_size = 0;
    other.m_levels = 1;
}

Cubemap& Cubemap::operator=(Cubemap&& other) noexcept {
    if (this != &other) {
        destroy();

        id = other.id;
        m_size = other.m_size;
        m_levels = other.m_levels;

        other.id = 0;
        other.m_size = 0;
        other.m_levels = 1;
    }
    return *this;
}

void Cubemap::destroy() {
    if (id != 0) {
        glDeleteTextures(1, &id);
        id = 0;
    }
    m_size = 0;
    m_levels = 1;
}

int Cubemap::compute_mip_levels(int size) {
    return 1 + static_cast<int>(std::floor(std::log2(static_cast<float>(size))));
}

GLenum Cubemap::toGLMagFilter(MagFilter f) {
    switch (f) {
        case MagFilter::Nearest: return GL_NEAREST;
        case MagFilter::Linear:  return GL_LINEAR;
        default:                 return GL_LINEAR;
    }
}

std::pair<GLenum, bool> Cubemap::toGLMinFilter(MinFilter f) {
    switch (f) {
        case MinFilter::Nearest:              return { GL_NEAREST, false };
        case MinFilter::Linear:               return { GL_LINEAR, false };
        case MinFilter::NearestMipmapNearest: return { GL_NEAREST_MIPMAP_NEAREST, true };
        case MinFilter::LinearMipmapNearest:  return { GL_LINEAR_MIPMAP_NEAREST, true };
        case MinFilter::NearestMipmapLinear:  return { GL_NEAREST_MIPMAP_LINEAR, true };
        case MinFilter::LinearMipmapLinear:   return { GL_LINEAR_MIPMAP_LINEAR, true };
        default:                              return { GL_LINEAR, false };
    }
}

void Cubemap::createEmpty(int size,
                          GLenum internal_format,
                          MagFilter mag_filter,
                          MinFilter min_filter) {
    destroy();

    const auto [minFilterGL, needsMips] = toGLMinFilter(min_filter);
    const GLenum magFilterGL = toGLMagFilter(mag_filter);

    m_size = size;
    m_levels = needsMips ? compute_mip_levels(size) : 1;

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &id);
    glTextureStorage2D(id, m_levels, internal_format, size, size);

    glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(id, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, minFilterGL);
    glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, magFilterGL);
}

bool Cubemap::loadFromFaces(const std::array<std::string, 6>& facepaths,
                            MagFilter mag_filter,
                            MinFilter min_filter,
                            bool srgb,
                            bool flipY) {
    destroy();

    const auto [minFilterGL, needsMips] = toGLMinFilter(min_filter);
    const GLenum magFilterGL = toGLMagFilter(mag_filter);

    stbi_set_flip_vertically_on_load(flipY ? 1 : 0);

    int firstW = 0;
    int firstH = 0;
    int firstChannels = 0;
    bool firstIsHDR = false;

    GLenum format = GL_RGB;
    GLenum internal = GL_RGB8;
    GLenum type = GL_UNSIGNED_BYTE;

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &id);

    for (int face = 0; face < 6; ++face) {
        const std::string& path = facepaths[face];
        const bool isHDR = stbi_is_hdr(path.c_str()) != 0;

        int w = 0, h = 0, channels = 0;

        if (face == 0) {
            firstIsHDR = isHDR;

            if (isHDR) {
                float* data = stbi_loadf(path.c_str(), &w, &h, &channels, 0);
                if (!data) {
                    std::cerr << "Failed to load cubemap face: " << path << "\n";
                    destroy();
                    stbi_set_flip_vertically_on_load(0);
                    return false;
                }

                if (w != h) {
                    std::cerr << "Cubemap face is not square: " << path << "\n";
                    stbi_image_free(data);
                    destroy();
                    stbi_set_flip_vertically_on_load(0);
                    return false;
                }

                firstW = w;
                firstH = h;
                firstChannels = channels;

                if (channels == 1) {
                    format = GL_RED;
                    internal = GL_R16F;
                } else if (channels == 3) {
                    format = GL_RGB;
                    internal = GL_RGB16F;
                } else if (channels == 4) {
                    format = GL_RGBA;
                    internal = GL_RGBA16F;
                } else {
                    std::cerr << "Unsupported channel count in cubemap face: " << path << "\n";
                    stbi_image_free(data);
                    destroy();
                    stbi_set_flip_vertically_on_load(0);
                    return false;
                }

                type = GL_FLOAT;
                m_size = w;
                m_levels = needsMips ? compute_mip_levels(w) : 1;

                glTextureStorage2D(id, m_levels, internal, w, h);

                glPixelStorei(GL_UNPACK_ALIGNMENT, (channels == 3) ? 1 : 4);
                glTextureSubImage3D(id, 0, 0, 0, face, w, h, 1, format, type, data);

                stbi_image_free(data);
            } else {
                unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 0);
                if (!data) {
                    std::cerr << "Failed to load cubemap face: " << path << "\n";
                    destroy();
                    stbi_set_flip_vertically_on_load(0);
                    return false;
                }

                if (w != h) {
                    std::cerr << "Cubemap face is not square: " << path << "\n";
                    stbi_image_free(data);
                    destroy();
                    stbi_set_flip_vertically_on_load(0);
                    return false;
                }

                firstW = w;
                firstH = h;
                firstChannels = channels;

                if (channels == 1) {
                    format = GL_RED;
                    internal = GL_R8;
                } else if (channels == 3) {
                    format = GL_RGB;
                    internal = srgb ? GL_SRGB8 : GL_RGB8;
                } else if (channels == 4) {
                    format = GL_RGBA;
                    internal = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
                } else {
                    std::cerr << "Unsupported channel count in cubemap face: " << path << "\n";
                    stbi_image_free(data);
                    destroy();
                    stbi_set_flip_vertically_on_load(0);
                    return false;
                }

                type = GL_UNSIGNED_BYTE;
                m_size = w;
                m_levels = needsMips ? compute_mip_levels(w) : 1;

                glTextureStorage2D(id, m_levels, internal, w, h);

                glPixelStorei(GL_UNPACK_ALIGNMENT, (channels == 3) ? 1 : 4);
                glTextureSubImage3D(id, 0, 0, 0, face, w, h, 1, format, type, data);

                stbi_image_free(data);
            }
        } else {
            if (isHDR != firstIsHDR) {
                std::cerr << "Cubemap faces mix HDR and LDR images, which is not supported.\n";
                destroy();
                stbi_set_flip_vertically_on_load(0);
                return false;
            }

            if (firstIsHDR) {
                float* data = stbi_loadf(path.c_str(), &w, &h, &channels, 0);
                if (!data) {
                    std::cerr << "Failed to load cubemap face: " << path << "\n";
                    destroy();
                    stbi_set_flip_vertically_on_load(0);
                    return false;
                }

                if (w != firstW || h != firstH || channels != firstChannels) {
                    std::cerr << "Cubemap faces do not match in size/channels: " << path << "\n";
                    stbi_image_free(data);
                    destroy();
                    stbi_set_flip_vertically_on_load(0);
                    return false;
                }

                glPixelStorei(GL_UNPACK_ALIGNMENT, (channels == 3) ? 1 : 4);
                glTextureSubImage3D(id, 0, 0, 0, face, w, h, 1, format, type, data);

                stbi_image_free(data);
            } else {
                unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 0);
                if (!data) {
                    std::cerr << "Failed to load cubemap face: " << path << "\n";
                    destroy();
                    stbi_set_flip_vertically_on_load(0);
                    return false;
                }

                if (w != firstW || h != firstH || channels != firstChannels) {
                    std::cerr << "Cubemap faces do not match in size/channels: " << path << "\n";
                    stbi_image_free(data);
                    destroy();
                    stbi_set_flip_vertically_on_load(0);
                    return false;
                }

                glPixelStorei(GL_UNPACK_ALIGNMENT, (channels == 3) ? 1 : 4);
                glTextureSubImage3D(id, 0, 0, 0, face, w, h, 1, format, type, data);

                stbi_image_free(data);
            }
        }
    }

    if (needsMips && m_levels > 1) {
        glGenerateTextureMipmap(id);
    }

    glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(id, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, minFilterGL);
    glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, magFilterGL);

    stbi_set_flip_vertically_on_load(0);
    return true;
}

void Cubemap::bind(int slot) const {
    glBindTextureUnit(slot, id);
}