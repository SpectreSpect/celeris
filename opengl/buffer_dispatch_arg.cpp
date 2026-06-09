#include "buffer_dispatch_arg.h"

BufferDispatchArg::BufferDispatchArg(const BufferObject* arg_buffer, uint32_t offset_bytes, uint32_t workgroup_size) : 
DispatchArg(arg_buffer, offset_bytes, 1u, workgroup_size) {

}