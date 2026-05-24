#include "instanced_render_object.h"
#include "../vulkan_self/vulkan_engine.h"

InstancedRenderObject::InstancedRenderObject(VulkanEngine& engine, Mesh& mesh, MaterialInstance& material, 
                                             uint32_t instance_count, uint32_t instance_size_bytes) 
    :   RenderObject(mesh, material),
        instance_data(engine, instance_count, instance_size_bytes){
}