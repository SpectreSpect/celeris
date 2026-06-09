#pragma once

#include <type_traits>
#include <cstdint>

#include "../../logger/logger_header.h"
#include "pass_object.h"
#include "../../slot_buffer.h"

class VulkanEngine;

class SlotPassObject : virtual public PassObject {
public:
    SlotPassObject(
        VulkanEngine& engine, 
        PipelinePass& pass,
        uint32_t slot_size, 
        uint32_t num_slots = 1000
    );
    virtual ~SlotPassObject() noexcept override = default;

    SlotPassObject(const SlotPassObject&) = delete;
    SlotPassObject& operator=(const SlotPassObject&) = delete;

    SlotPassObject(SlotPassObject&&) noexcept = default;
    SlotPassObject& operator=(SlotPassObject&&) noexcept = default;

    SlotBuffer& slot_buffer() noexcept;
    const SlotBuffer& slot_buffer() const noexcept;

    template <class SlotType>
    void set_slot(uint32_t slot_id, const SlotType& data) {
        LOG_NAMED("SlotPassObject");

        static_assert(
            std::is_trivially_copyable_v<SlotType>,
            "Slot buffer slot types should be trivially copyable"
        );

        m_slot_buffer.update_slot<SlotType>(slot_id, data);
    }

    void sync();

private:
    SlotBuffer m_slot_buffer;
};
