#pragma once
#include "program.h"

class Material {
public:
    Program* program = nullptr;
    Material() = default;
    Material(Program* program);
    
    void bind();
};