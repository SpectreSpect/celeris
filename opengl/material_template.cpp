#include "material_template.h"

MaterialTemplate::MaterialTemplate(VfProgram* program)
    : program(program) {}

MaterialTemplate::MaterialTemplate(std::string vs_path, std::string fs_path) {
    // VertexShader* vertex_shader = new VertexShader(vs_path);
    // FragmentShader* fragment_shader = new FragmentShader(fs_path);

    // program = new Program(vertex_shader, fragment_shader);
}

void MaterialTemplate::bind() const {
    if (program)
        program->use();
}

bool MaterialTemplate::has_param(const std::string& name) const {
    for (const auto& p : params)
        if (p.name == name)
            return true;
    return false;
}
