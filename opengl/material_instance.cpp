#include "material_instance.h"

MaterialInstance::MaterialInstance(MaterialTemplate* templ)
    : templ(templ) {}

void MaterialInstance::set(const std::string& name, MaterialValue value) {
    if (!templ) {
        std:: cerr << "MaterialInstance::set: no template\n";
        return;
    }

    auto itdesc  = std::find_if(templ->params.begin(), templ->params.end(),
                               [&](const MaterialParamDesc& d){ return d.name == name; });
    
    if (itdesc == templ->params.end()) {
        std::cerr << "Warning: param '" << name << "' not declared in template\n";
        return;
    }

    bool ok = false;
    switch (itdesc->type) {
        case MaterialParamType::Float:
            ok = std::holds_alternative<float>(value);
            break;
        case MaterialParamType::Vec3:
            ok = std::holds_alternative<glm::vec3>(value);
            break;
        case MaterialParamType::Mat4:
            ok = std::holds_alternative<glm::mat4>(value);
            break;
    }

    if (!ok) {
        std::cerr << "Type mismatch for param '" << name << "'\n";
        return;
    }

    values[name] = value;
}

void MaterialInstance::bind() const {
    if (!templ || !templ->program)
        return;

    templ->bind();

    int texture_unit = 0;

    for (const auto& desc : templ->params) {
        auto it = values.find(desc.name);
        if (it == values.end()) continue; // no instance value provided

        const MaterialValue& v = it->second;
        switch (desc.type) {
            // case MaterialParamType::Float:
            //     if (auto p = std::get_if<float>(&v))
            //         templ->program->set_float(desc.name, *p);
            //     break;

            case MaterialParamType::Vec3:
                if (auto p = std::get_if<glm::vec3>(&v))
                    templ->program->set_vec3(desc.name, *p);
                break;

            case MaterialParamType::Mat4:
                if (auto p = std::get_if<glm::mat4>(&v))
                    templ->program->set_mat4(desc.name, *p);
                break;

            // case MaterialParamType::Texture2D:
            //     if (auto p = std::get_if<Texture*>(&v); p && *p) {
            //         (*p)->bind(texture_unit);
            //         templ->program->set_int(desc.name, texture_unit);
            //         ++texture_unit;
            //     }
            //     break;
        }
    }
}
