#pragma once

#include <cstdint>
#include <utility>

#include "scene_object.h"

#include "transform.h"
#include "mesh.h"

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/pass/instance/slot_pass_instance.h"

class MeshView;

class RenderObject : public SceneObject {
public:
    _XCHILD_NAME(RenderObject);

    RenderObject(Mesh& mesh, SlotPassInstance& material);
    RenderObject(MeshView mesh_view, SlotPassInstance& material);
    ~RenderObject();

    RenderObject(const RenderObject&) = delete;
    RenderObject& operator=(const RenderObject&) = delete;

    RenderObject(RenderObject&& other) noexcept;
    RenderObject& operator=(RenderObject&& other) noexcept;

    template<class SlotType>
    void set_material_data(const SlotType& data) {
        logger.check(m_material != nullptr, "RenderObject has no material");

        m_material->slot_buffer().update_slot<SlotType>(
            m_material_data_id,
            data
        );
    }

    template<class SlotType, class Fn>
    void edit_material_data(Fn&& fn) {
        logger.check(m_material != nullptr, "RenderObject has no material");

        m_material->slot_buffer().edit_slot<SlotType>(
            m_material_data_id,
            std::forward<Fn>(fn)
        );
    }

    template<class SlotType>
    void set_material(SlotPassInstance& new_material, const SlotType& data) {
        if (m_material) {
            m_material->slot_buffer().free_slot(m_material_data_id);
        }

        m_material = &new_material;
        m_material_data_id = new_material.slot_buffer().create_slot<SlotType>(data);
    }

    void sync_material();

    MeshView& mesh_view() noexcept;
    SlotPassInstance& material() noexcept;
    uint32_t material_data_id() const noexcept;
    
    virtual void render(Renderer& renderer, VulkanCommandBuffer& command_buffer, const glm::mat4& world_transform);

private:
    // Mesh& m_mesh;
    MeshView m_mesh_view;
    SlotPassInstance* m_material = nullptr;
    uint32_t m_material_data_id = UINT32_MAX;
};
