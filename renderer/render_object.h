#pragma once

#include <cstdint>
#include <utility>

#include "scene_object.h"

#include "transform.h"
#include "mesh.h"

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/material/material_instance.h"


class RenderObject : public SceneObject {
public:
    _XCLASS_NAME(RenderObject);

    RenderObject(Mesh& mesh, MaterialInstance& material)
        : m_mesh(mesh),
          m_material(&material),
          material_data_id(material.material_buffer.allocate_slot())
    {}

    ~RenderObject();

    RenderObject(const RenderObject&) = delete;
    RenderObject& operator=(const RenderObject&) = delete;

    RenderObject(RenderObject&& other) noexcept;
    RenderObject& operator=(RenderObject&& other) noexcept;

    template<class SlotType>
    void set_material_data(const SlotType& data) {
        logger.check(m_material != nullptr, "RenderObject has no material");

        m_material->material_buffer.update_slot<SlotType>(
            material_data_id,
            data
        );
    }

    template<class SlotType, class Fn>
    void edit_material_data(Fn&& fn) {
        logger.check(m_material != nullptr, "RenderObject has no material");

        m_material->material_buffer.edit_slot<SlotType>(
            material_data_id,
            std::forward<Fn>(fn)
        );
    }

    template<class SlotType>
    void set_material(MaterialInstance& new_material, const SlotType& data) {
        if (m_material) {
            m_material->material_buffer.free_slot(material_data_id);
        }

        m_material = &new_material;
        material_data_id = new_material.material_buffer.create_slot<SlotType>(data);
    }

    void sync_material();

    // RenderObject& add_child(RenderObject& child);
    
    virtual void render(Renderer& renderer, VulkanCommandBuffer& command_buffer, const glm::mat4& world_transform);
    

    Mesh& m_mesh;
    MaterialInstance* m_material = nullptr;
    uint32_t material_data_id = UINT32_MAX;

    // RenderObject* parent = nullptr;
    // std::vector<RenderObject*> children;
};