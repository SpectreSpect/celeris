#include "slot_pass_instance.h"

#include "../../descriptor_set/descriptor_set.h"

SlotPassInstance::SlotPassInstance(
    VulkanEngine& engine,
    DescriptorPool& pool,
    PipelinePass& pass,
    uint32_t slot_size,
    uint32_t num_slots,
    uint32_t instance_set_id,
    uint32_t slot_buffer_bind_id)
    :   PassObject(pass),
        SlotPassObject(
            engine,
            pass,
            slot_size,
            num_slots
        ),
        PassInstance(pass, pool, instance_set_id),
        m_slot_buffer_bind_id(slot_buffer_bind_id) 
    {
        LOG_METHOD();
        descripter_set().write_storage_buffer(m_slot_buffer_bind_id, slot_buffer().gpu_buffer());
    }
