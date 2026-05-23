#pragma once

#include "../vulkan_buffer.h"
#include "../logger/logger_header.h"

template<class SlotType>
class MaterialBuffer {
public:
    _XCLASS_NAME(MaterialBuffer);

    MaterialBuffer(VulkannEngine& engine, uint32_t num_slots) 
    :   material_buffer_gpu(VulkanBuffer::create_storage_buffer(engine, num_slots * sizeof(SlotType))) {
        material_buffer_cpu.resize(num_slots);
        slot_alive.resize(num_slots);
        slot_dirty.resize(num_slots);
    }

    std::vector<SlotType> material_buffer_cpu;

    uint32_t get_new_slot() {
        LOG_METHOD();

        uint32_t slot_id;

        if (!free_slots.empty()) {
            slot_id = free_slots.back();
            free_slots.pop_back();

            logger.check(slot_id < slot_alive.size(), "Slot id was out of bounds");
            logger.check(!slot_alive[slot_id], "Slot was in free_slots but is already alive");

            slot_alive[slot_id] = true;
        } else {
            slot_id = static_cast<uint32_t>(material_buffer_cpu.size());

            material_buffer_cpu.emplace_back();
            slot_alive.push_back(true);
            slot_dirty.push_back(false);
        }

        mark_dirty(slot_id);
        return slot_id;
    }

    void free_slot(uint32_t slot_id) {
        LOG_METHOD();

        logger.check(slot_id < slot_alive.size(), "Slot id was out of bounds");
        logger.check(slot_alive[slot_id], "Double free or invalid free");

        slot_alive[slot_id] = false;
        free_slots.push_back(slot_id);
    }

    void update_slot(uint32_t slot_id, const SlotType& data) {
        LOG_METHOD();

        logger.check(slot_id < material_buffer_cpu.size(), "Slot id was out of bounds");
        logger.check(slot_alive[slot_id], "Trying to update a freed material slot");

        material_buffer_cpu[slot_id] = data;
        mark_dirty(slot_id);
    }

    void update_slot(uint32_t slot_id, const SlotType& data) {
        material_buffer_cpu[slot_id] = data;
        dirty_slots.push_back(slot_id);
    }

    void sync() {
        LOG_METHOD();

        for (uint32_t slot_id : dirty_slots) { 
            material_buffer_gpu.upload<SlotType>(material_buffer_cpu[slot_id], slot_id);
            slot_dirty = false;
        }

        dirty_slots.clear();
    }

private:
    VulkanBuffer material_buffer_gpu;

    std::vector<uint32_t> free_slots; // free slots
    std::vector<uint32_t> dirty_slots; // slots that should be updated
    
    std::vector<uint8_t> slot_alive;
    std::vector<uint8_t> slot_dirty;

    void mark_dirty(uint32_t slot_id) {
        LOG_METHOD();

        logger.check(slot_id < material_buffer_cpu.size(), "Slot id was out of bounds");
        logger.check(slot_id < slot_dirty.size(), "Slot id was out of bounds");

        if (!slot_dirty[slot_id]) {
            slot_dirty[slot_id] = true;
            dirty_slots.push_back(slot_id);
        }
    }
};