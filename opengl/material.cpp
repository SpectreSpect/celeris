#include "material.h"

Material::Material(Program* program) {
    this->program = program;
}

void Material::bind() {
    if (!program)
        return;
    
    program->use();
}