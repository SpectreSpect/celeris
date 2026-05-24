#include "renderer.h"
#include "transform_push_constants.h"
#include "render_object.h"
#include "instanced_render_object.h"
#include "../vulkan_self/vulkan_command_buffer.h"
#include "resources/frame_resources.h"

Renderer::Renderer(VulkanEngine& engine, FrameResources& frame_resources) {
    m_engine = &engine;
    m_frame_resources = &frame_resources;
}

void Renderer::render(VulkanCommandBuffer& command_buffer, RenderObject& render_object) {
    static TransformPushConstants pc;
    pc.model = render_object.transform.get_model_matrix();
    pc.material_data_id = render_object.material_data_id;

    // blinn_phong_material_instance.bind(command_buffer);
    render_object.m_material->bind(command_buffer);
    MaterialPass& pass = render_object.m_material->m_pass;

    m_frame_resources->bind(m_engine->current_frame(), command_buffer, pass.pipeline(), 1);

    pass.pipeline().set_y_up_viewport(command_buffer, *m_engine);
    pass.pipeline().set_scissor(command_buffer, *m_engine);

    render_object.m_mesh.bind_vertex_buffer(command_buffer);
    render_object.m_mesh.bind_index_buffer(command_buffer);

    pass.pipeline_layout().push_constants(command_buffer, pc);
    
    command_buffer.draw_indexed(render_object.m_mesh.index_count());
}


void Renderer::render(VulkanCommandBuffer& command_buffer, InstancedRenderObject& render_object) {
        static TransformPushConstants pc;
        pc.model = render_object.transform.get_model_matrix();
        pc.material_data_id = render_object.material_data_id;

        // blinn_phong_material_instance.bind(command_buffer);
        render_object.m_material->bind(command_buffer);
        MaterialPass& pass = render_object.m_material->m_pass;

        m_frame_resources->bind(m_engine->current_frame(), command_buffer, pass.pipeline(), 1);

        pass.pipeline().set_y_up_viewport(command_buffer, *m_engine);
        pass.pipeline().set_scissor(command_buffer, *m_engine);

        render_object.m_mesh.bind_vertex_buffer(command_buffer, 0);
        render_object.instance_data.buffer().bind_as_vertex_buffer(command_buffer, 1);

        render_object.m_mesh.bind_index_buffer(command_buffer);

        pass.pipeline_layout().push_constants(command_buffer, pc);
        
        command_buffer.draw_indexed(render_object.m_mesh.index_count(), render_object.instance_data.instance_count());
    };