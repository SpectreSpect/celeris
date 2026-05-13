#include "pbr_renderer.h"
#include "../mesh.h"

PBRRenderer::PBRRenderer(VulkanEngine& engine, VulkanShaderModule& vertex_shader, VulkanShaderModule& fragment_shader, float exposure) {
    this->vertex_shader = &vertex_shader;
    this->fragment_shader = &fragment_shader;
    this->engine = &engine;
    this->exposure = exposure;

    VulkanVertexLayout vertex_layout;
    LayoutInitializer layout_initializer = vertex_layout.get_initializer();
    layout_initializer.add(AttrFormat::FLOAT4);
    layout_initializer.add(AttrFormat::FLOAT4);
    layout_initializer.add(AttrFormat::FLOAT4);
    layout_initializer.add(AttrFormat::FLOAT2);
    layout_initializer.add(AttrFormat::FLOAT4);
    VkPipelineVertexInputStateCreateInfo vertexInput = vertex_layout.get_vertex_intput();

    uniform_buffer = VideoBuffer(engine, sizeof(PBRUniform));

    DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    // builder.add_combined_image_sampler(1, VK_SHADER_STAGE_FRAGMENT_BIT); // albedo
    // builder.add_combined_image_sampler(2, VK_SHADER_STAGE_FRAGMENT_BIT); // env_map
    // builder.add_combined_image_sampler(3, VK_SHADER_STAGE_FRAGMENT_BIT); // irradiance_map
    // builder.add_combined_image_sampler(1, VK_SHADER_STAGE_FRAGMENT_BIT); // irradianceMap
    // builder.add_combined_image_sampler(2, VK_SHADER_STAGE_FRAGMENT_BIT); // prefilterMap
    // builder.add_combined_image_sampler(3, VK_SHADER_STAGE_FRAGMENT_BIT); // uBrdfLUT
    builder.add_combined_image_sampler(1, VK_SHADER_STAGE_FRAGMENT_BIT); // TEST!!!!!!
    builder.add_storage_buffer(4, VK_SHADER_STAGE_FRAGMENT_BIT); // LightSources
    builder.add_storage_buffer(5, VK_SHADER_STAGE_FRAGMENT_BIT); // NumLightsInClusters
    builder.add_storage_buffer(6, VK_SHADER_STAGE_FRAGMENT_BIT); // LightsInClusters
    descriptor_set_bundle = builder.create(engine.device);

    pipeline = GraphicsPipeline(engine, descriptor_set_bundle, vertex_layout, vertex_shader, fragment_shader);
}


void PBRRenderer::render(
    Drawable& drawable,
    Camera& camera,
    Texture2D& albedo_texture,
    LightingSystem& lighting_system)
{
    if (!engine) {
        std::string message = "'engine' was nullptr";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    RenderState render_state = {};

    float aspect = float(engine->swapchainExtent.width) / float(engine->swapchainExtent.height);
    render_state.proj = camera.get_projection_matrix(aspect);
    render_state.vp = render_state.proj * camera.get_view_matrix();
    render_state.engine = engine;
    render_state.transform = glm::mat4(1.0f);
    render_state.renderer = this;
    render_state.camera = &camera;
    render_state.view = camera.get_view_matrix();
    // render_state.prefilte_map_mip_levels = prefilter_map.image_resource.mip_levels - 1;
    render_state.prefilte_map_mip_levels = 6;
    render_state.lighting_system = &lighting_system;
    render_state.viewport_px = {engine->swapchainExtent.width, engine->swapchainExtent.height};
    

    // descriptor_set_bundle.bind_combined_image_sampler(1, irradiance_map);
    // descriptor_set_bundle.bind_combined_image_sampler(2, prefilter_map);
    // descriptor_set_bundle.bind_combined_image_sampler(3, brdf_lut);
    descriptor_set_bundle.bind_combined_image_sampler(1, albedo_texture);

    descriptor_set_bundle.bind_storage_buffer(4, lighting_system.light_source_ssbo);
    descriptor_set_bundle.bind_storage_buffer(5, lighting_system.num_lights_in_clusters_ssbo);
    descriptor_set_bundle.bind_storage_buffer(6, lighting_system.lights_in_clusters_ssbo);


    // layout(std430, set = 0, binding = 4) readonly buffer LightSourcesBuffer {
    //     LightSource light_sources[];
    // };

    // layout(std430, set = 0, binding = 5) readonly buffer NumLightsInClustersBuffer {
    //     uint num_lights_in_clusters[];
    // };

    // layout(std430, set = 0, binding = 6) readonly buffer LightsInClustersBuffer {
    //     uint lights_in_clusters[];
    // };

    PBRUniform ubo{};

    ubo.view = render_state.view;
    ubo.proj = render_state.proj;

    ubo.viewPos = glm::vec4(render_state.camera->position, 1.0f);
    ubo.environmentMultiplier = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    ubo.pbrParams.y = render_state.prefilte_map_mip_levels;
    ubo.pbrParams.z = exposure;
    
    ubo.clusterGrid.x = render_state.lighting_system->num_clusters.x;
    ubo.clusterGrid.y = render_state.lighting_system->num_clusters.y;
    ubo.clusterGrid.z = render_state.lighting_system->num_clusters.z;
    ubo.clusterGrid.w = render_state.lighting_system->max_lights_per_cluster;
    ubo.screenParams.x = render_state.viewport_px.x;
    ubo.screenParams.y = render_state.viewport_px.y;
    ubo.screenParams.z = render_state.camera->near_plane;
    ubo.screenParams.w = render_state.camera->near_plane;

    uniform_buffer.update_data(&ubo, sizeof(ubo));

    drawable.draw(render_state);
}

void PBRRenderer::render_mesh(Mesh& mesh, RenderState state) {
    glm::mat4 model = state.transform * mesh.get_model_matrix();

    state.engine->bind_pipeline(pipeline);
    state.engine->bind_vertex_buffer(mesh.vertex_buffer);
    state.engine->bind_index_buffer(mesh.index_buffer);

    vkCmdPushConstants(
        state.engine->currentCommandBuffer,
        pipeline.pipeline_layout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(glm::mat4),
        &model
    );

    state.engine->draw_indexed(mesh.num_indices);
}