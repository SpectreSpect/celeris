#pragma once

class VulkanEngine;
class ShaderManager;
class TextureManager;
class MaterialManager;
class MaterialInstanceManager;
class MeshManager;

class ManagerBundle {
public:
    ManagerBundle(VulkanEngine& engine, ShaderManager& shader_manager, TextureManager& texture_manager, 
                  MaterialManager& material_manager, MaterialInstanceManager& material_instance_manager, MeshManager& mesh_manager);

    VulkanEngine& engine() noexcept;
    ShaderManager& shader_manager() noexcept;
    TextureManager& texture_manager() noexcept;
    MaterialManager& material_manager() noexcept;
    MaterialInstanceManager& material_instance_manager() noexcept;
    MeshManager& mesh_manager() noexcept;
private:
    VulkanEngine& m_engine;
    ShaderManager& m_shader_manager;
    TextureManager& m_texture_manager;
    MaterialManager& m_material_manager;
    MaterialInstanceManager& m_material_instance_manager;
    MeshManager& m_mesh_manager;
};