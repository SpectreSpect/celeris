#pragma once
#include <unordered_map>
#include <variant>

#include <glm/glm.hpp>

#include "material_template.h"
#include <algorithm>
// #include "texture.h"

using MaterialValue = std::variant<
    float,
    glm::vec3,
    glm::mat4
    // Texture*
>;

class MaterialInstance {
public:
    MaterialTemplate* templ = nullptr;

    std::unordered_map<std::string, MaterialValue> values;

    MaterialInstance() = default;
    MaterialInstance(MaterialTemplate* templ);

    void set(const std::string& name, MaterialValue value);
    void bind() const;
};
