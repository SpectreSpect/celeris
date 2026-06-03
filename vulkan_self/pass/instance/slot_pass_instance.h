#pragma once

#include <cstdint>

#include "slot_pass_object.h"
#include "pass_instance.h"

#include "../../logger/logger_header.h"

class PipelinePass;
class VulkanEngine;
class DescriptorPool;
class VulkanCommandBuffer;

class SlotPassInstance : public SlotPassObject, public PassInstance {
public:
    _XCHILD_NAME(PassInstance);

    SlotPassInstance(
        VulkanEngine& engine,
        DescriptorPool& pool,
        PipelinePass& pass,
        uint32_t slot_size,
        uint32_t num_slots = 1000,
        uint32_t instance_set_id = 0,
        uint32_t slot_buffer_bind_id = 0
    );

private:
    uint32_t m_slot_buffer_bind_id = 0;
};
