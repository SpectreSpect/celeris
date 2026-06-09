#include "dispatch_arg.h"

DispatchArg::DispatchArg(const BufferObject* arg_buffer, uint32_t offset_bytes, uint32_t direct_value, uint32_t workgroup_size) : 
arg_buffer(arg_buffer), offset_bytes(offset_bytes), direct_value(direct_value), workgroup_size(workgroup_size) {
    
}