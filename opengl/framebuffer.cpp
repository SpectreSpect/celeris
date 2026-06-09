#include "framebuffer.h"

Framebuffer::Framebuffer(int width, int height) {
    create(width, height);
}

Framebuffer::~Framebuffer() {
    destroy();
}

Framebuffer::Framebuffer(Framebuffer&& other) noexcept {
    moveFrom(other);
}

Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept {
    if (this != &other) {
        destroy();
        moveFrom(other);
    }
    return *this;
}

void Framebuffer::moveFrom(Framebuffer& other) noexcept {
    id = other.id;
    depth_rbo = other.depth_rbo;
    width = other.width;
    height = other.height;

    other.id = 0;
    other.depth_rbo = 0;
    other.width = 0;
    other.height = 0;
}

void Framebuffer::create(int w, int h) {
    destroy();

    width = w;
    height = h;

    glCreateFramebuffers(1, &id);
}

void Framebuffer::destroy() {
    if (depth_rbo != 0) {
        glDeleteRenderbuffers(1, &depth_rbo);
        depth_rbo = 0;
    }

    if (id != 0) {
        glDeleteFramebuffers(1, &id);
        id = 0;
    }

    width = 0;
    height = 0;
}

void Framebuffer::bind(GLenum target) const {
    glBindFramebuffer(target, id);
}

void Framebuffer::unbind(GLenum target) {
    glBindFramebuffer(target, 0);
}

void Framebuffer::attachTexture2D(GLuint texture_id,
                                  GLenum attachment,
                                  GLint level) {
    glNamedFramebufferTexture(id, attachment, texture_id, level);
}

void Framebuffer::attachCubemapFace(GLuint cubemap_id,
                                    int face,
                                    GLenum attachment,
                                    GLint level) {
    // face: 0..5, in OpenGL cubemap order:
    // 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    glNamedFramebufferTextureLayer(id, attachment, cubemap_id, level, face);
}

void Framebuffer::createDepthRenderbuffer(GLenum internal_format) {
    if (depth_rbo != 0) {
        glDeleteRenderbuffers(1, &depth_rbo);
        depth_rbo = 0;
    }

    glCreateRenderbuffers(1, &depth_rbo);
    glNamedRenderbufferStorage(depth_rbo, internal_format, width, height);
    glNamedFramebufferRenderbuffer(id, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rbo);
}

void Framebuffer::createDepthStencilRenderbuffer(GLenum internal_format) {
    if (depth_rbo != 0) {
        glDeleteRenderbuffers(1, &depth_rbo);
        depth_rbo = 0;
    }

    glCreateRenderbuffers(1, &depth_rbo);
    glNamedRenderbufferStorage(depth_rbo, internal_format, width, height);
    glNamedFramebufferRenderbuffer(id, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth_rbo);
}

void Framebuffer::setDrawBuffer(GLenum attachment) {
    glNamedFramebufferDrawBuffer(id, attachment);
}

void Framebuffer::setDrawBuffers(const std::vector<GLenum>& attachments) {
    glNamedFramebufferDrawBuffers(id,
                                  static_cast<GLsizei>(attachments.size()),
                                  attachments.data());
}

void Framebuffer::disableColorBuffer() {
    glNamedFramebufferDrawBuffer(id, GL_NONE);
    glNamedFramebufferReadBuffer(id, GL_NONE);
}

bool Framebuffer::isComplete(GLenum target) const {
    GLenum status = glCheckNamedFramebufferStatus(id, target);

    if (status == GL_FRAMEBUFFER_COMPLETE) {
        return true;
    }

    std::cerr << "Framebuffer incomplete: ";

    switch (status) {
        case GL_FRAMEBUFFER_UNDEFINED:
            std::cerr << "GL_FRAMEBUFFER_UNDEFINED";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            std::cerr << "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            std::cerr << "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            std::cerr << "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            std::cerr << "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            std::cerr << "GL_FRAMEBUFFER_UNSUPPORTED";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            std::cerr << "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
            std::cerr << "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
            break;
        default:
            std::cerr << "Unknown status (" << status << ")";
            break;
    }

    std::cerr << "\n";
    return false;
}