#include "material_manager.h"

MaterialTemplate* MaterialManager::get(const std::string& name) {
    return material_templates.at(name).get();
}

MaterialTemplate* MaterialManager::load_blinn_phong() {
    auto it = material_templates.find("blinn_phong");
    if (it != material_templates.end())
        return it->second.get();
    
    material_templates["blinn_phong"] = std::make_unique<MaterialTemplate>(
                                                        (executable_dir() / "shaders" / "blinn_phong_vs.glsl").string(),
                                                        (executable_dir() / "shaders" / "blinn_phong_fs.glsl").string()
                                                    );

    auto* material_template = material_templates["blinn_phong"].get();
                                                    
    material_template->params.push_back({"uMVP", MaterialParamType::Mat4});
    material_template->params.push_back({"uModel", MaterialParamType::Mat4});
    material_template->params.push_back({"uViewPos", MaterialParamType::Vec3});

    return material_template;
}