#pragma once
#include "dispatch_arg.h"

class ValueDispatchArg : public DispatchArg {
public:
    ValueDispatchArg(uint32_t direct_value, uint32_t workgroup_size = USE_DEFAULT_WORKGROUP_SIZE);
};