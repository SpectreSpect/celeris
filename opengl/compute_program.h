#pragma once
#include "program.h"
#include "compute_shader.h"

class ComputeProgram : public Program {
public:
    ComputeProgram();
    ComputeProgram(ComputeShader* compute_shader);

    ComputeProgram(const ComputeProgram&) = delete;
    ComputeProgram& operator=(const ComputeProgram&) = delete;
    ComputeProgram(ComputeProgram&& o) noexcept { id = o.id; o.id = 0; }
    ComputeProgram& operator=(ComputeProgram&& o) noexcept {
        if (this != &o) {
            if (id) glDeleteProgram(id);
            id = o.id;
            o.id = 0;
        }
        return *this;
    }

    void dispatch_compute(GLuint groups_x, GLuint groups_y, GLuint groups_z);
};