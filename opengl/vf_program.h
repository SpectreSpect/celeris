#pragma once
#include "program.h"
#include "vertex_shader.h"
#include "fragment_shader.h"

class VfProgram : public Program {
public:
    VfProgram() = default;
    VfProgram(VertexShader* vertex_shader, FragmentShader* fragment_shader);

    VfProgram(const VfProgram&) = delete;
    VfProgram& operator=(const VfProgram&) = delete;
    VfProgram(VfProgram&& o) noexcept;
    VfProgram& operator=(VfProgram&& o) noexcept;
};