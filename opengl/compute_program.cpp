#include "compute_program.h"

ComputeProgram::ComputeProgram() {
    
}

ComputeProgram::ComputeProgram(ComputeShader* compute_shader) : Program({compute_shader}) {

}

void ComputeProgram::dispatch_compute(GLuint groups_x, GLuint groups_y, GLuint groups_z) {
    use();
    glDispatchCompute(groups_x, groups_y, groups_z);
}