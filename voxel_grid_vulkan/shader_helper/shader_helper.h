#pragma once

#include "value_dispatch_arg.h"
#include "../../vulkan_self/pass/instance/pass_writer.h"

class VulkanDevice;
class VulkanBuffer;
class ComputePassManager;

class ShaderHelper {
public:
    ShaderHelper(VulkanDevice& device, ComputePassManager& compute_pass_manager);
    
    
    void prepare_dispatch_args(
        VulkanCommandBuffer& command_buffer,
        VulkanBuffer& dispatch_args,
        const DispatchArg& arg_x = ValueDispatchArg(1u),
        const DispatchArg& arg_y = ValueDispatchArg(1u),
        const DispatchArg& arg_z = ValueDispatchArg(1u)
    );

private:
    PassWriter m_dispatch_adapter_pw;
};