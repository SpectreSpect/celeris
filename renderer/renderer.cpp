#include "renderer.h"
#include "transform_push_constants.h"
#include "render_object.h"
#include "instanced_render_object.h"
#include "../vulkan_self/vulkan_command_buffer.h"
#include "resources/frame_resources.h"
#include "scene.h"

Renderer::Renderer(VulkanEngine& engine, FrameResources& frame_resources) {
    m_engine = &engine;
    m_frame_resources = &frame_resources;
}

void Renderer::render(VulkanCommandBuffer& command_buffer, RenderObject& render_object, glm::mat4 transform) {
    static TransformPushConstants pc;
    pc.model = transform;
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

void Renderer::render(VulkanCommandBuffer& command_buffer, InstancedRenderObject& render_object, glm::mat4 transform) {
        static TransformPushConstants pc;
        pc.model = transform;
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

void Renderer::render(VulkanCommandBuffer& command_buffer, std::vector<RenderObject*> render_objects, glm::mat4 transform) {
    for (RenderObject* render_object : render_objects) {
        glm::mat4 local_transform = render_object->transform.get_model_matrix();
        glm::mat4 world_transform = transform * local_transform;

        if (dynamic_cast<InstancedRenderObject*>(render_object))
            render(command_buffer, *(InstancedRenderObject*)render_object, world_transform);
        else
            render(command_buffer, *render_object, world_transform);

        render_object->m_material->material_buffer.sync();

        
        render(command_buffer, render_object->children, world_transform);
    }
}

void Renderer::render(VulkanCommandBuffer& command_buffer, Scene& scene) {
    render(command_buffer, scene.render_objects);
}