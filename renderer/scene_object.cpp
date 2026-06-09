#include "scene_object.h"

SceneObject::SceneObject() = default;

SceneObject::SceneObject(SceneObject&& other) noexcept
    : transform(std::move(other.transform)),
      parent(other.parent),
      children(std::move(other.children))
{
    replace_parent_child_pointer(&other, this);

    for (SceneObject* child : children) {
        if (child) {
            child->parent = this;
        }
    }

    other.parent = nullptr;
    other.children.clear();
}

SceneObject& SceneObject::operator=(SceneObject&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    detach_from_parent();
    detach_children();

    transform = std::move(other.transform);
    parent = other.parent;
    children = std::move(other.children);

    replace_parent_child_pointer(&other, this);

    for (SceneObject* child : children) {
        if (child) {
            child->parent = this;
        }
    }

    other.parent = nullptr;
    other.children.clear();

    return *this;
}

SceneObject& SceneObject::add_child(SceneObject& child) {
    child.detach_from_parent();

    child.parent = this;
    children.push_back(&child);

    return child;
}

void SceneObject::detach_from_parent() noexcept {
    if (!parent) {
        return;
    }

    auto& siblings = parent->children;

    siblings.erase(
        std::remove(siblings.begin(), siblings.end(), this),
        siblings.end()
    );

    parent = nullptr;
}

void SceneObject::detach_children() noexcept {
    for (SceneObject* child : children) {
        if (child && child->parent == this) {
            child->parent = nullptr;
        }
    }

    children.clear();
}

void SceneObject::replace_parent_child_pointer(
    SceneObject* old_ptr,
    SceneObject* new_ptr
) noexcept {
    if (!parent) {
        return;
    }

    for (SceneObject*& child : parent->children) {
        if (child == old_ptr) {
            child = new_ptr;
            return;
        }
    }
}