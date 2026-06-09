#pragma once
#include "vf_program.h"
#include "vector"
#include "string"

enum class MaterialParamType {
    Float,
    Vec3,
    Mat4,
    Texture2D
};

struct MaterialParamDesc {
    std::string name;          // uniform name in shader
    MaterialParamType type;
};

class MaterialTemplate {
public:
    VfProgram* program = nullptr;

    std::vector<MaterialParamDesc> params;
    
    MaterialTemplate() = default;
    MaterialTemplate(VfProgram* program);
    MaterialTemplate(std::string vs_path, std::string fs_path);

    void bind() const;

    bool has_param(const std::string& name) const;
};