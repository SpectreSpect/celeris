#include "manager_bundle.h"

#include "../vulkan_self/vulkan_engine.h"
#include "shader_manager.h"
#include "texture_manager.h"
#include "material_manager.h"
#include "material_instance_manager.h"
#include "mesh_manager.h"

ManagerBundle::ManagerBundle(VulkanEngine& engine, ShaderManager& shader_manager, TextureManager& texture_manager, 
                  MaterialManager& material_manager, MaterialInstanceManager& material_instance_manager, MeshManager& mesh_manager) 
    :   m_engine(engine),
        m_shader_manager(shader_manager),
        m_texture_manager(texture_manager),
        m_material_manager(material_manager),
        m_material_instance_manager(material_instance_manager),
        m_mesh_manager(mesh_manager) {}

VulkanEngine& ManagerBundle::engine() noexcept {
    return m_engine;
}

ShaderManager& ManagerBundle::shader_manager() noexcept {
    return m_shader_manager;
}

TextureManager& ManagerBundle::texture_manager() noexcept {
    return m_texture_manager;
}

MaterialManager& ManagerBundle::material_manager() noexcept {
    return m_material_manager;
}

MaterialInstanceManager& ManagerBundle::material_instance_manager() noexcept {
    return m_material_instance_manager;
}

MeshManager& ManagerBundle::mesh_manager() noexcept {
    return m_mesh_manager;
}