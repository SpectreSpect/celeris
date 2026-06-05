#include "shader_helper.h"

#include "../../vulkan_self/vulkan_buffer.h"
#include "../../vulkan_self/vulkan_command_buffer.h"
#include "../../vulkan_self/push_constants_structures.h"
#include "../../managers/compute_pass_manager.h"


ShaderHelper::ShaderHelper(VulkanDevice& device, ComputePassManager& compute_pass_manager)
    :   m_dispatch_adapter_pw(device, compute_pass_manager.dispatch_adapter_cp) {}

void ShaderHelper::prepare_dispatch_args(VulkanCommandBuffer& command_buffer, VulkanBuffer& dispatch_args, 
                                         const DispatchArg& arg_x, const DispatchArg& arg_y, const DispatchArg& arg_z)
{
    // if (arg_x.arg_buffer != nullptr) m_dispatch_adapter_pw.set_storage_buffer(0, *arg_x.arg_buffer);
    // if (arg_y.arg_buffer != nullptr) m_dispatch_adapter_pw.set_storage_buffer(1, *arg_y.arg_buffer);
    // if (arg_z.arg_buffer != nullptr) m_dispatch_adapter_pw.set_storage_buffer(2, *arg_z.arg_buffer);


    m_dispatch_adapter_pw.set_storage_buffer(0, arg_x.arg_buffer ? *arg_x.arg_buffer : dispatch_args);
    m_dispatch_adapter_pw.set_storage_buffer(1, arg_y.arg_buffer ? *arg_y.arg_buffer : dispatch_args);
    m_dispatch_adapter_pw.set_storage_buffer(2, arg_z.arg_buffer ? *arg_z.arg_buffer : dispatch_args);

    m_dispatch_adapter_pw.set_storage_buffer(3, dispatch_args);

    m_dispatch_adapter_pw.bind(command_buffer);

    uint32_t x_workgroup_size = arg_x.workgroup_size == DispatchArg::USE_DEFAULT_WORKGROUP_SIZE ? 256u : arg_x.workgroup_size;
    uint32_t y_workgroup_size = arg_y.workgroup_size == DispatchArg::USE_DEFAULT_WORKGROUP_SIZE ? 1u : arg_y.workgroup_size;
    uint32_t z_workgroup_size = arg_z.workgroup_size == DispatchArg::USE_DEFAULT_WORKGROUP_SIZE ? 1u : arg_z.workgroup_size;
    
    m_dispatch_adapter_pw.push_constants(command_buffer, DispatchAdapterPushConstants{
        .u_offset_bytes_0 = arg_x.offset_bytes,
        .u_offset_bytes_1 = arg_y.offset_bytes,
        .u_offset_bytes_2 = arg_z.offset_bytes,

        .u_direct_value_0 = arg_x.direct_value,
        .u_direct_value_1 = arg_y.direct_value,
        .u_direct_value_2 = arg_z.direct_value,

        .u_x_workgroup_size = x_workgroup_size,
        .u_y_workgroup_size = y_workgroup_size,
        .u_z_workgroup_size = z_workgroup_size
        });
    
    command_buffer.dispatch(1, 1, 1);

    dispatch_args.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}