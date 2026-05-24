#pragma once

#include <vector>
#include <algorithm>
#include <utility>

#include "transform.h"

#include "../../../vulkan_self/logger/logger.h"

class Renderer;
class VulkanCommandBuffer;

class SceneObject {
public:
    _XPARENT_NAME(SceneObject);

    Transform transform;

    SceneObject* parent = nullptr;
    std::vector<SceneObject*> children;

    SceneObject();

    virtual ~SceneObject() = default;

    SceneObject(const SceneObject&) = delete;
    SceneObject& operator=(const SceneObject&) = delete;

    SceneObject(SceneObject&& other) noexcept;
    SceneObject& operator=(SceneObject&& other) noexcept;

    SceneObject& add_child(SceneObject& child);

    virtual void render(
        Renderer& renderer,
        VulkanCommandBuffer& command_buffer,
        const glm::mat4& world_transform
    ) {}

private:
    void detach_from_parent() noexcept;
    void detach_children() noexcept;
    void replace_parent_child_pointer(SceneObject* old_ptr, SceneObject* new_ptr) noexcept;
};