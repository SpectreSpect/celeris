#pragma once

#include <vector>
#include <cstdint>
#include <utility>
#include <cstddef>
#include <cstring>
#include <type_traits>

#include "../vulkan_buffer.h"
#include "../logger/logger_header.h"

class VulkanEngine;

class MaterialBuffer {
public:
    _XCLASS_NAME(MaterialBuffer);

    MaterialBuffer(VulkanEngine& engine, uint32_t num_slots, uint32_t slot_size)
        : m_material_buffer_gpu(
              VulkanBuffer::create_host_visible_storage_buffer(
                  engine,
                  num_slots * slot_size
              )
          ),
          m_material_buffer_cpu(num_slots * slot_size),
          m_free_slots(num_slots),
          m_slot_alive(num_slots, false),
          m_slot_dirty(num_slots, false),
          m_num_slots(num_slots),
          m_slot_size(slot_size)
    {
        LOG_METHOD();

        for (uint32_t i = 0; i < num_slots; i++) {
            m_free_slots[i] = num_slots - i - 1;
        }
    }


    template<class SlotType>
    static MaterialBuffer create(VulkanEngine& engine, uint32_t num_slots) {
        static_assert(
            std::is_trivially_copyable_v<SlotType>,
            "Material buffer slot types should be trivially copyable"
        );

        return MaterialBuffer(engine, num_slots, sizeof(SlotType));
    }

    template<class SlotType>
    SlotType read(uint32_t slot_id) const {
        LOG_METHOD();

        check_slot_type<SlotType>();
        check_alive_slot(slot_id, "Trying to read a freed material slot");

        SlotType result;
        std::memcpy(
            &result,
            slot_ptr(slot_id),
            sizeof(SlotType)
        );

        return result;
    }

    template<class SlotType, class Fn>
    void edit_slot(uint32_t slot_id, Fn&& fn) {
        LOG_METHOD();

        SlotType data = read<SlotType>(slot_id);

        std::forward<Fn>(fn)(data);

        update_slot<SlotType>(slot_id, data);
    }

    template<class SlotType>
    uint32_t create_slot(const SlotType& data) {
        LOG_METHOD();

        check_slot_type<SlotType>();

        uint32_t slot_id = allocate_slot();
        update_slot<SlotType>(slot_id, data);

        return slot_id;
    }

    template<class SlotType>
    void update_slot(uint32_t slot_id, const SlotType& data) {
        LOG_METHOD();

        static_assert(
            std::is_trivially_copyable_v<SlotType>,
            "Material buffer slot types should be trivially copyable"
        );

        check_slot_type<SlotType>();
        check_alive_slot(slot_id, "Trying to update a freed material slot");

        std::memcpy(
            slot_ptr(slot_id),
            &data,
            sizeof(SlotType)
        );

        mark_dirty(slot_id);
    }

    uint32_t allocate_slot() {
        LOG_METHOD();

        logger.check(!m_free_slots.empty(), "MaterialBuffer is out of free slots");

        uint32_t slot_id = m_free_slots.back();
        m_free_slots.pop_back();

        logger.check(slot_id < m_slot_alive.size(), "Slot id was out of bounds");
        logger.check(!m_slot_alive[slot_id], "Slot was in free_slots but is already alive");

        m_slot_alive[slot_id] = true;

        return slot_id;
    }

    void free_slot(uint32_t slot_id) {
        LOG_METHOD();

        logger.check(slot_id < m_slot_alive.size(), "Slot id was out of bounds");
        logger.check(m_slot_alive[slot_id], "Double free or invalid free");

        m_slot_alive[slot_id] = false;
        m_free_slots.push_back(slot_id);
    }

    void sync() {
        LOG_METHOD();

        for (uint32_t slot_id : m_dirty_slots) {
            logger.check(slot_id < m_slot_dirty.size(), "Slot id was out of bounds");

            if (m_slot_alive[slot_id]) {
                sync_slot(slot_id);
            } else {
                m_slot_dirty[slot_id] = false;
            }
        }

        m_dirty_slots.clear();
    }

    VulkanBuffer& gpu_buffer() noexcept {
        return m_material_buffer_gpu;
    }

    const VulkanBuffer& gpu_buffer() const noexcept {
        return m_material_buffer_gpu;
    }

    uint32_t slot_size() const noexcept {
        return m_slot_size;
    }

    uint32_t slot_count() const noexcept {
        return m_num_slots;
    }

private:
    VulkanBuffer m_material_buffer_gpu;
    std::vector<std::byte> m_material_buffer_cpu;

    std::vector<uint32_t> m_free_slots;
    std::vector<uint32_t> m_dirty_slots;

    std::vector<uint8_t> m_slot_alive;
    std::vector<uint8_t> m_slot_dirty;

    uint32_t m_num_slots = 0;
    uint32_t m_slot_size = 0;

private:
    template<class SlotType>
    void check_slot_type() const {
        logger.check(
            sizeof(SlotType) == m_slot_size,
            "Wrong material slot type used with this MaterialBuffer"
        );
    }

    void check_alive_slot(uint32_t slot_id, std::string_view message) const {
        logger.check(slot_id < m_num_slots, "Slot id was out of bounds");
        logger.check(m_slot_alive[slot_id], message);
    }

    std::byte* slot_ptr(uint32_t slot_id) {
        return m_material_buffer_cpu.data() + slot_id * m_slot_size;
    }

    const std::byte* slot_ptr(uint32_t slot_id) const {
        return m_material_buffer_cpu.data() + slot_id * m_slot_size;
    }

    void mark_dirty(uint32_t slot_id) {
        LOG_METHOD();

        logger.check(slot_id < m_num_slots, "Slot id was out of bounds");

        if (!m_slot_dirty[slot_id]) {
            m_slot_dirty[slot_id] = true;
            m_dirty_slots.push_back(slot_id);
        }
    }

    void sync_slot(uint32_t slot_id) {
        LOG_METHOD();

        check_alive_slot(slot_id, "Trying to sync a freed material slot");

        m_material_buffer_gpu.upload(
            slot_ptr(slot_id),
            m_slot_size,
            slot_id * m_slot_size
        );

        m_slot_dirty[slot_id] = false;
    }
};