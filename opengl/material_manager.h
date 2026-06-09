#pragma once
#include "unordered_map"
#include "iostream"
#include "material_template.h"
#include "path_utils.h"
#include "memory"


class MaterialManager {
public:
    std::unordered_map<std::string, std::unique_ptr<MaterialTemplate>> material_templates;

    MaterialTemplate* get(const std::string& name);
    MaterialTemplate* load_blinn_phong();
};