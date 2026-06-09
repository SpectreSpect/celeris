#pragma once

#include <GL/glew.h>
#include <iostream>
#include <vector>

class Framebuffer {
public:
    GLuint id = 0;
    GLuint depth_rbo = 0;

    int width = 0;
    int height = 0;

    Framebuffer() = default;
    Framebuffer(int width, int height);

    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;

    void create(int width, int height);
    void destroy();

    void bind(GLenum target = GL_FRAMEBUFFER) const;
    static void unbind(GLenum target = GL_FRAMEBUFFER);

    void attachTexture2D(GLuint texture_id,
                         GLenum attachment = GL_COLOR_ATTACHMENT0,
                         GLint level = 0);

    void attachCubemapFace(GLuint cubemap_id,
                           int face,
                           GLenum attachment = GL_COLOR_ATTACHMENT0,
                           GLint level = 0);

    void createDepthRenderbuffer(GLenum internal_format = GL_DEPTH_COMPONENT24);
    void createDepthStencilRenderbuffer(GLenum internal_format = GL_DEPTH24_STENCIL8);

    void setDrawBuffer(GLenum attachment);
    void setDrawBuffers(const std::vector<GLenum>& attachments);
    void disableColorBuffer();

    bool isComplete(GLenum target = GL_FRAMEBUFFER) const;

private:
    void moveFrom(Framebuffer& other) noexcept;
};