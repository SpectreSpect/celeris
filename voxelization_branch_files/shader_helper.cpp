#include "shader_helper.h"

ShaderHelper::ShaderHelper(ShaderManager* shader_manager) : shader_manager(shader_manager) {
    init_programs();
}

void ShaderHelper::init_programs() {
    prog_dispatch_adapter_ = ComputeProgram(&shader_manager->dispatch_adapter_cs);
}

void ShaderHelper::prepare_dispatch_args(BufferObject& dispatch_args, const DispatchArg& arg_x, const DispatchArg& arg_y, const DispatchArg& arg_z)
{
    if (arg_x.arg_buffer != nullptr) arg_x.arg_buffer->bind_base_as_ssbo(0);
    if (arg_y.arg_buffer != nullptr) arg_y.arg_buffer->bind_base_as_ssbo(1);
    if (arg_z.arg_buffer != nullptr) arg_z.arg_buffer->bind_base_as_ssbo(2);

    dispatch_args.bind_base_as_ssbo(3);

    prog_dispatch_adapter_.use();
    glUniform1ui(glGetUniformLocation(prog_dispatch_adapter_.id, "u_offset_bytes_0"), arg_x.offset_bytes);
    glUniform1ui(glGetUniformLocation(prog_dispatch_adapter_.id, "u_offset_bytes_1"), arg_y.offset_bytes);
    glUniform1ui(glGetUniformLocation(prog_dispatch_adapter_.id, "u_offset_bytes_2"), arg_z.offset_bytes);

    glUniform1ui(glGetUniformLocation(prog_dispatch_adapter_.id, "u_direct_value_0"), arg_x.direct_value);
    glUniform1ui(glGetUniformLocation(prog_dispatch_adapter_.id, "u_direct_value_1"), arg_y.direct_value);
    glUniform1ui(glGetUniformLocation(prog_dispatch_adapter_.id, "u_direct_value_2"), arg_z.direct_value);

    uint32_t x_workgroup_size = arg_x.workgroup_size == DispatchArg::USE_DEFAULT_WORKGROUP_SIZE ? 256u : arg_x.workgroup_size;
    uint32_t y_workgroup_size = arg_y.workgroup_size == DispatchArg::USE_DEFAULT_WORKGROUP_SIZE ? 1u : arg_y.workgroup_size;
    uint32_t z_workgroup_size = arg_z.workgroup_size == DispatchArg::USE_DEFAULT_WORKGROUP_SIZE ? 1u : arg_z.workgroup_size;

    glUniform1ui(glGetUniformLocation(prog_dispatch_adapter_.id, "u_x_workgroup_size"), x_workgroup_size);
    glUniform1ui(glGetUniformLocation(prog_dispatch_adapter_.id, "u_y_workgroup_size"), y_workgroup_size);
    glUniform1ui(glGetUniformLocation(prog_dispatch_adapter_.id, "u_z_workgroup_size"), z_workgroup_size);

    prog_dispatch_adapter_.dispatch_compute(1u, 1u, 1u);
    glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
}