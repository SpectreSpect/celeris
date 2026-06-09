#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


class GPUTimestamp {
public:
    GLuint id;

    GPUTimestamp();
    ~GPUTimestamp();

    GPUTimestamp(const GPUTimestamp&) noexcept = delete;
    GPUTimestamp& operator=(const GPUTimestamp&) noexcept = delete;

    GPUTimestamp(GPUTimestamp&& other) noexcept;
    GPUTimestamp& operator=(GPUTimestamp&& other) noexcept;

    uint64_t ns() const;
    double operator-(const GPUTimestamp& other) const;
};