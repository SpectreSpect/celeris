#include "vf_program.h"

VfProgram::VfProgram(VertexShader* vertex_shader, FragmentShader* fragment_shader) 
: Program({vertex_shader, fragment_shader}) {

}

VfProgram::VfProgram(VfProgram&& o) noexcept {
    id = o.id;
    o.id = 0;
}

VfProgram& VfProgram::operator=(VfProgram&& o) noexcept {
    if (this != &o) {
        if (id) glDeleteProgram(id);
        id = o.id;
        o.id = 0;
    }
    return *this;
}