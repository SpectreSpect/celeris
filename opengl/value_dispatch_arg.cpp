#include "value_dispatch_arg.h"

ValueDispatchArg::ValueDispatchArg(uint32_t direct_value, uint32_t workgroup_size) : 
DispatchArg(nullptr, USE_DIRECT_VALUE, direct_value, workgroup_size) {

}