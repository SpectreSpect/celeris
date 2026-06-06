#pragma once
#include "dispatch_arg.h"

class BufferDispatchArg : public DispatchArg {
public:
    BufferDispatchArg(VulkanBuffer* arg_buffer, uint32_t offset_bytes = 0, uint32_t workgroup_size = USE_DEFAULT_WORKGROUP_SIZE);
};