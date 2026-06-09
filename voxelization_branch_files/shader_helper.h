#pragma once
#include "shader_manager.h"
#include "buffer_object.h"
#include "dispatch_arg.h"
#include "value_dispatch_arg.h"
#include "buffer_dispatch_arg.h"

class ShaderHelper {
public:
    ShaderManager* shader_manager = nullptr;

    ShaderHelper(ShaderManager* shader_manager);

    void prepare_dispatch_args(
        BufferObject& dispatch_args,
        const DispatchArg& arg_x = ValueDispatchArg(1u),
        const DispatchArg& arg_y = ValueDispatchArg(1u),
        const DispatchArg& arg_z = ValueDispatchArg(1u)
    );

private:
    ComputeProgram prog_dispatch_adapter_;

    void init_programs();
};