#include "material_manager.h"

#include "../vulkan_self/vulkan_engine.h"
#include "shader_manager.h"
#include "../renderer/resources/frame_resources.h"
#include "../vulkan_self/pass/material_pass/material_pass_builder.h"
#include "../vulkan_self/vulkan_shader_module.h"
#include "../vulkan_self/formats.h"
#include "../renderer/transform_push_constants.h"
#include "../vulkan_self/image/vulkan_texture_2d.h"
#include "../renderer/material_data_types.h"
#include "../vulkan_self/image/cubemap.h"
#include "../vulkan_self/image/cubemap_array.h"
#include "../renderer/material_structures.h"
#include "../renderer/lines/line_instance.h"

MaterialManager::MaterialManager(VulkanEngine& engine, ShaderManager& shader_manager, FrameResources& frame_resources)
:   blin_phong_mp(create_blin_phong_pass(engine, frame_resources, shader_manager.blinn_phong_vs, shader_manager.blinn_phong_fs)),
    unlit_mp(create_unlit_pass(engine, frame_resources, shader_manager.unlit_vs, shader_manager.unlit_fs)),
    point_mp(create_point_pass(engine, frame_resources, shader_manager.point_vs, shader_manager.point_fs)),
    line_mp(create_line_pass(engine, frame_resources, shader_manager.line_vs, shader_manager.line_fs)),
    skybox_mp(create_skybox_pass(engine, frame_resources, shader_manager.skybox_vs, shader_manager.skybox_fs)),
    pbr_mp(create_pbr_pass(engine, frame_resources, shader_manager.pbr_vs, shader_manager.pbr_fs)),
    voxel_mesh_mp(create_voxel_mesh_pass(engine, frame_resources, shader_manager.voxel_mesh_vs, shader_manager.voxel_mesh_fs)),
    voxel_pbr_mp(create_voxel_pbr_pass(engine, frame_resources, shader_manager.voxel_pbr_vs, shader_manager.voxel_pbr_fs)),
    m_pool(engine.device(), m_pool_builder) {}

MaterialPass MaterialManager::create_pass(VulkanEngine& engine, MaterialPassBuilder& builder, 
                                          const VulkanShaderModule& vs, const VulkanShaderModule& fs) {
    builder.add_vertex_shader(vs);
    builder.add_fragment_shader(fs);
    
    m_pool_builder.add_layout(builder.material_dsl_builder(), m_max_material_instances);
    
    return MaterialPass(engine, builder);
}

DescriptorPool& MaterialManager::descriptor_pool() noexcept {
    return m_pool;
}

MaterialPass MaterialManager::create_blin_phong_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                                     const VulkanShaderModule& vs, const VulkanShaderModule& fs) {
    LOG_METHOD();

    struct BlinPhongVertex {
        glm::vec4 position;
        glm::vec4 normal;
        glm::vec2 uv;
        glm::vec4 tangent;
    };

    MaterialPassBuilder builder;

    builder.add_storage_buffer(0, ShaderStages::fragment);
    // builder.add_uniform_buffer(1, ShaderStages::fragment);
    builder.add_combined_image_sampler(1, ShaderStages::fragment);
    builder.add_uniform_buffer(2, ShaderStages::fragment);
    builder.add_storage_buffer(3, ShaderStages::fragment);

    builder.add_push_constants(sizeof(TransformPushConstants), 0);
    builder.add_descriptor_set_layout(frame_resources.descriptor_layout());

    builder.add_vertex_binding(0, sizeof(BlinPhongVertex));
    builder.add_vertex_attribute(0, 0, Formats::vec4, offsetof(BlinPhongVertex, position));
    builder.add_vertex_attribute(1, 0, Formats::vec4, offsetof(BlinPhongVertex, normal));
    builder.add_vertex_attribute(2, 0, Formats::vec2, offsetof(BlinPhongVertex, uv));
    builder.add_vertex_attribute(3, 0, Formats::vec4, offsetof(BlinPhongVertex, tangent));

    return create_pass(engine, builder, vs, fs);
}


MaterialPass MaterialManager::create_unlit_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                                const VulkanShaderModule& vs, const VulkanShaderModule& fs) {
    LOG_METHOD();

    struct UnlitVertex {
        glm::vec4 position;
        glm::vec4 normal;
        glm::vec2 uv;
    };

    MaterialPassBuilder builder;

    // builder.add_uniform_buffer(0, ShaderStages::fragment);
    builder.add_storage_buffer(0, ShaderStages::fragment);
    // builder.add_combined_image_sampler(1, ShaderStages::fragment);

    builder.add_push_constants(sizeof(TransformPushConstants), 0);
    builder.add_descriptor_set_layout(frame_resources.descriptor_layout());

    builder.add_vertex_binding(0, sizeof(UnlitVertex));
    builder.add_vertex_attribute(0, 0, Formats::vec4, offsetof(UnlitVertex, position));
    builder.add_vertex_attribute(1, 0, Formats::vec4, offsetof(UnlitVertex, normal));
    builder.add_vertex_attribute(2, 0, Formats::vec2, offsetof(UnlitVertex, uv));

    return create_pass(engine, builder, vs, fs);
}

MaterialPass MaterialManager::create_point_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                                const VulkanShaderModule& vs, const VulkanShaderModule& fs) {
    LOG_METHOD();

    struct PointVertex {
        glm::vec2 corner;
    };

    struct alignas(16) PointInstance {
        glm::vec4 pos;
        glm::vec4 color;
    };

    MaterialPassBuilder builder;

    builder.add_storage_buffer(0, ShaderStages::fragment);
    builder.add_uniform_buffer(1, ShaderStages::vertex);
    // builder.add_combined_image_sampler(1, ShaderStages::fragment);

    builder.add_push_constants(sizeof(TransformPushConstants), 0);
    builder.add_descriptor_set_layout(frame_resources.descriptor_layout());

    builder.add_vertex_binding(0, sizeof(PointVertex), VK_VERTEX_INPUT_RATE_VERTEX);
    builder.add_vertex_binding(1, sizeof(PointInstance), VK_VERTEX_INPUT_RATE_INSTANCE);

    builder.add_vertex_attribute(0, 0, Formats::vec2, offsetof(PointVertex, corner));

    builder.add_vertex_attribute(1, 1, Formats::vec4, offsetof(PointInstance, pos));
    builder.add_vertex_attribute(2, 1, Formats::vec4, offsetof(PointInstance, color));

    return create_pass(engine, builder, vs, fs);
}

MaterialPass MaterialManager::create_line_pass(
    VulkanEngine& engine, 
    FrameResources& frame_resources, 
    const VulkanShaderModule& vs, 
    const VulkanShaderModule& fs)
{
    LOG_METHOD();

    MaterialPassBuilder builder;

    builder.add_storage_buffer(0, ShaderStages::vertex);

    builder.add_push_constants(sizeof(TransformPushConstants), 0);
    builder.add_descriptor_set_layout(frame_resources.descriptor_layout());

    builder.add_vertex_binding(0, sizeof(LineVertex), VK_VERTEX_INPUT_RATE_VERTEX);
    builder.add_vertex_binding(1, sizeof(LineInstance), VK_VERTEX_INPUT_RATE_INSTANCE);

    builder.add_vertex_attribute(0, 0, Formats::vec2, offsetof(LineVertex, corner));

    builder.add_vertex_attribute(1, 1, Formats::vec3, offsetof(LineInstance, p0));
    builder.add_vertex_attribute(2, 1, Formats::vec3, offsetof(LineInstance, p1));
    builder.add_vertex_attribute(3, 1, Formats::vec4, offsetof(LineInstance, color));

    return create_pass(engine, builder, vs, fs);
}

MaterialPass MaterialManager::create_skybox_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                                const VulkanShaderModule& vs, const VulkanShaderModule& fs) {
    LOG_METHOD();

    struct SkyboxVertex {
        glm::vec4 position;
    };

    MaterialPassBuilder builder;

    builder.add_storage_buffer(0, ShaderStages::fragment);
    builder.add_combined_image_sampler(1, ShaderStages::fragment);

    builder.add_push_constants(sizeof(TransformPushConstants), 0);
    builder.add_descriptor_set_layout(frame_resources.descriptor_layout());

    builder.add_vertex_binding(0, sizeof(SkyboxVertex));
    builder.add_vertex_attribute(0, 0, Formats::vec4, offsetof(SkyboxVertex, position));

    return create_pass(engine, builder, vs, fs);
}

MaterialPass MaterialManager::create_pbr_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                                const VulkanShaderModule& vs, const VulkanShaderModule& fs) {
    LOG_METHOD();

    MaterialPassBuilder builder;

    builder.add_storage_buffer(0, ShaderStages::fragment);
    builder.add_combined_image_sampler(1, ShaderStages::fragment); // irradianceMap
    builder.add_combined_image_sampler(2, ShaderStages::fragment); // prefilterMap
    builder.add_combined_image_sampler(3, ShaderStages::fragment); // brdfLUT

    builder.add_push_constants(sizeof(TransformPushConstants), 0);
    builder.add_descriptor_set_layout(frame_resources.descriptor_layout());

    builder.add_vertex_binding(0, sizeof(PBRVertex));
    builder.add_vertex_attribute(0, 0, Formats::vec4, offsetof(PBRVertex, position));
    builder.add_vertex_attribute(1, 0, Formats::vec4, offsetof(PBRVertex, normal));
    builder.add_vertex_attribute(2, 0, Formats::vec2, offsetof(PBRVertex, uv));
    builder.add_vertex_attribute(3, 0, Formats::vec4, offsetof(PBRVertex, tangent));

    return create_pass(engine, builder, vs, fs);
}

MaterialPass MaterialManager::create_voxel_mesh_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                                const VulkanShaderModule& vs, const VulkanShaderModule& fs) {
    LOG_METHOD();

    // struct MeshVoxelVertex {
    //     glm::vec4 position;
    //     uint32_t color;
    //     uint32_t face;
    // };

    struct alignas(16) MeshVoxelVertex {
        glm::vec4 position; // offset 0
        uint32_t color;    // offset 16
        uint32_t face;     // offset 20
        uint32_t _pad0;    // offset 24
        uint32_t _pad1;    // offset 28
    };

    static_assert(sizeof(MeshVoxelVertex) == 32);
    static_assert(offsetof(MeshVoxelVertex, position) == 0);
    static_assert(offsetof(MeshVoxelVertex, color) == 16);
    static_assert(offsetof(MeshVoxelVertex, face) == 20);

    MaterialPassBuilder builder;

    builder.add_storage_buffer(0, ShaderStages::fragment);
    // builder.add_combined_image_sampler(1, ShaderStages::fragment); // irradianceMap
    // builder.add_combined_image_sampler(2, ShaderStages::fragment); // prefilterMap
    // builder.add_combined_image_sampler(3, ShaderStages::fragment); // brdfLUT

    builder.add_push_constants(sizeof(TransformPushConstants), 0);
    builder.add_descriptor_set_layout(frame_resources.descriptor_layout());

    builder.add_vertex_binding(0, sizeof(MeshVoxelVertex));
    builder.add_vertex_attribute(0, 0, Formats::vec4, offsetof(MeshVoxelVertex, position));
    builder.add_vertex_attribute(1, 0, VK_FORMAT_R32_UINT, offsetof(MeshVoxelVertex, color));
    builder.add_vertex_attribute(2, 0, VK_FORMAT_R32_UINT, offsetof(MeshVoxelVertex, face));

    return create_pass(engine, builder, vs, fs);
}

MaterialPass MaterialManager::create_voxel_pbr_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                                const VulkanShaderModule& vs, const VulkanShaderModule& fs) {
    LOG_METHOD();

    struct alignas(16) MeshVoxelVertex {
        glm::vec4 position;
        uint32_t color;
        uint32_t face;
        uint32_t _pad0;
        uint32_t _pad1;
    };

    static_assert(sizeof(MeshVoxelVertex) == 32);
    static_assert(offsetof(MeshVoxelVertex, position) == 0);
    static_assert(offsetof(MeshVoxelVertex, color) == 16);
    static_assert(offsetof(MeshVoxelVertex, face) == 20);

    MaterialPassBuilder builder;

    builder.add_storage_buffer(0, ShaderStages::fragment);
    builder.add_combined_image_sampler(1, ShaderStages::fragment);
    builder.add_combined_image_sampler(2, ShaderStages::fragment);
    builder.add_combined_image_sampler(3, ShaderStages::fragment);

    builder.add_push_constants(sizeof(TransformPushConstants), 0);
    builder.add_descriptor_set_layout(frame_resources.descriptor_layout());

    builder.add_vertex_binding(0, sizeof(MeshVoxelVertex));
    builder.add_vertex_attribute(0, 0, Formats::vec4, offsetof(MeshVoxelVertex, position));
    builder.add_vertex_attribute(1, 0, VK_FORMAT_R32_UINT, offsetof(MeshVoxelVertex, color));
    builder.add_vertex_attribute(2, 0, VK_FORMAT_R32_UINT, offsetof(MeshVoxelVertex, face));

    return create_pass(engine, builder, vs, fs);
}

SlotPassInstance MaterialManager::create_blinn_phong_material(VulkanEngine& engine, VulkanTexture2D& albedo){
    SlotPassInstance material(engine, m_pool, blin_phong_mp, sizeof(BlinPhongMaterialData));
    
    material.set_texture(1, albedo);

    return material;
}

SlotPassInstance MaterialManager::create_skybox_material(VulkanEngine& engine, Cubemap& skybox_cubemap) {
    SlotPassInstance material(engine, m_pool, skybox_mp, sizeof(SkyboxMaterialData));
    
    material.descripter_set().write_cubemap(1, skybox_cubemap);

    return material;
}

SlotPassInstance MaterialManager::create_pbr_material(VulkanEngine& engine, CubemapArray& irradiance_maps, CubemapArray& prefilter_maps, VulkanTexture2D& brdf_lut) {
    SlotPassInstance material(engine, m_pool, pbr_mp, sizeof(PBRMaterialData));

    material.descripter_set().write_cubemap_array(1, irradiance_maps);
    material.descripter_set().write_cubemap_array(2, prefilter_maps);
    material.set_texture(3, brdf_lut);

    return material;
}

SlotPassInstance MaterialManager::create_voxel_pbr_material(VulkanEngine& engine, CubemapArray& irradiance_maps, CubemapArray& prefilter_maps, VulkanTexture2D& brdf_lut) {
    SlotPassInstance material(engine, m_pool, voxel_pbr_mp, sizeof(PBRMaterialData));

    material.descripter_set().write_cubemap_array(1, irradiance_maps);
    material.descripter_set().write_cubemap_array(2, prefilter_maps);
    material.set_texture(3, brdf_lut);

    return material;
}
