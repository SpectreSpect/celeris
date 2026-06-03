#include "slot_pass_object.h"

#include "../../vulkan_engine.h"

SlotPassObject::SlotPassObject(
    VulkanEngine& engine,
    PipelinePass& pass,
    uint32_t slot_size,
    uint32_t num_slots)
    :   PassObject(pass),
        m_slot_buffer(engine, num_slots, slot_size)
{
    LOG_NAMED("SlotPassObject");

    logger.check(slot_size > 0, "Slot size must be greater than 0");
    logger.check(num_slots > 0, "Attempt to create slot buffer with 0 slots");
}

SlotBuffer& SlotPassObject::slot_buffer() noexcept {
    return m_slot_buffer;
}

const SlotBuffer& SlotPassObject::slot_buffer() const noexcept {
    return m_slot_buffer;
}

void SlotPassObject::sync() {
    LOG_NAMED("SlotPassObject");

    m_slot_buffer.sync();
}
