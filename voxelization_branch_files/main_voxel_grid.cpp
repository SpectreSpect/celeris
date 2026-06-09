#define ALLOW_LOCK_BufferObject

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

#include "engine3d.h"
#include "imgui_layer.h"
#include "voxel_grid_gpu.h"
#include "voxel_grid_gpu_debugger.h"
#include "torus.h"
#include "cube.h"

#include "voxel_rasterizator_gpu.h"

float clear_col[4] = {0.776470588f, 0.988235294f, 1.0f, 1.0f};

BufferObject create_voxel_write_rect(glm::ivec3 origin, glm::uvec3 rect_dim, glm::vec3 color) {
    uint32_t count_voxels = rect_dim.x * rect_dim.y * rect_dim.z;
    std::vector<VoxelWriteGPU> voxels(count_voxels);

    uint32_t id = 0;
    for (uint32_t x = 0u; x < rect_dim.x; x++)
        for (uint32_t y = 0u; y < rect_dim.y; y++)
            for (uint32_t z = 0u; z < rect_dim.z; z++) {
                double ton = 0.5 + ((double)(rand() % 100000) / 100000) * 0.5;
                voxels[id].world_voxel = glm::ivec4(origin + glm::ivec3(x, y, z), 0);
                voxels[id].voxel_data = std::move(VoxelDataGPU(1, VOXEL_VISABILITY_FLAG_BIT, glm::ivec3(color * glm::vec3(ton))));
                voxels[id].set_flags = OVERWRITE_BIT;
                id++;
            }
    
    BufferObject voxel_writes(sizeof(uint32_t) * 4 + sizeof(VoxelWriteGPU) * count_voxels, GL_STATIC_COPY);
    voxel_writes.update_subdata(0, sizeof(uint32_t), &count_voxels);
    voxel_writes.update_subdata(sizeof(uint32_t) * 4, sizeof(VoxelWriteGPU) * count_voxels, voxels.data());

    return voxel_writes;
}

int main() {
    Engine3D engine;
    Window window(&engine, 1280, 720, "3D visualization");
    engine.set_window(&window);

    std::filesystem::path debug_shaders_root_path = executable_dir() / "debug_shaders";
    ShaderManager shader_manager = ShaderManager(executable_dir(), &shaders_inlude_directories, &debug_shaders_root_path);

    ShaderHelper shader_helper(&shader_manager);

    Camera camera;
    window.set_camera(&camera);

    FPSCameraController camera_controller = FPSCameraController(&camera);
    camera_controller.speed = 100;

    Mesh torus = Torus::create_torus_mesh();
    glm::vec3 torus_origin_pos = glm::vec3(0, 70, 0);
    torus.position = torus_origin_pos;
    torus.scale = glm::vec3(30);
    torus.rotation = glm::vec3(0, 0, 0);

    glm::vec3 voxel_size(1.0f);
    glm::ivec3 chunk_size(16);
    VoxelGridGPU::VoxelGridDesc voxel_grid_desc;
    voxel_grid_desc.chunk_size = chunk_size;
    voxel_grid_desc.voxel_size = voxel_size;
    voxel_grid_desc.count_active_chunks = 10'000;
    voxel_grid_desc.max_quads = 3'000'000;
    voxel_grid_desc.chunk_hash_table_size_factor = 1.0f;
    voxel_grid_desc.count_evict_buckets = 32;
    voxel_grid_desc.min_free_chunks = 4'500;
    voxel_grid_desc.tomb_fraction_to_rebuild = 0.2f;
    voxel_grid_desc.eviction_bucket_shell_thickness = chunk_size.x * voxel_size.x * 1;
    voxel_grid_desc.vb_page_size_order_of_two = 10;
    voxel_grid_desc.ib_page_size_order_of_two = 10;
    voxel_grid_desc.buddy_allocator_nodes_factor = 1.0;
    voxel_grid_desc.render_distance = chunk_size.x * voxel_size.x * 30;
    voxel_grid_desc.generation_distance = 10;
    voxel_grid_desc.max_write_count = chunk_size.x * chunk_size.y * chunk_size.z * 2'000;

    VoxelGridGPU voxel_grid_gpu(voxel_grid_desc, &shader_helper, &shader_manager);
    VoxelGridGPUDebugger voxel_grid_debugger(&voxel_grid_gpu, &shader_helper, &window);

    VoxelRasterizatorGPU::VoxelRasterizatorDesc voxel_rasterizator_desc;
    voxel_rasterizator_desc.chunk_size = glm::ivec3(4);
    voxel_rasterizator_desc.voxel_size = voxel_size;
    voxel_rasterizator_desc.counter_hash_table_size = 50'000;
    voxel_rasterizator_desc.count_voxel_writes = 0;
    VoxelRasterizatorGPU voxel_rasterizator(voxel_rasterizator_desc, &voxel_grid_gpu, &shader_manager, &shader_helper);

    VoxelWriteGPU blue_voxel_prifab;
    blue_voxel_prifab.voxel_data = VoxelDataGPU(1u, VOXEL_VISABILITY_FLAG_BIT, glm::ivec3(66, 135, 245));
    blue_voxel_prifab.set_flags = OVERWRITE_BIT;

    VoxelWriteGPU air_voxel_prifab;
    air_voxel_prifab.voxel_data = VoxelDataGPU(0u, VOXEL_EASY_OVERWRITE_FLAG_BIT, glm::ivec3(255));
    air_voxel_prifab.set_flags = OVERWRITE_BIT;

    float rotation_speed = glm::pi<float>() / 2.0f;

    int y_offset = 0;
    float timer = 0;
    float lastFrame = 0;
    float wait_timer = 0;

    glm::vec3 torus_offset = glm::vec3(0);
    glm::ivec3 chunk_pos(0);
    while(window.is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - lastFrame;
        timer += delta_time;
        wait_timer += delta_time;
        lastFrame = currentFrame;   

        ui::begin_frame();
        ui::update_mouse_mode(&window);

        camera_controller.update(&window, delta_time);

        ///////////////////////////////////////////////////////////////////////////////////////////////////
        voxel_rasterizator.rasterize(torus, blue_voxel_prifab, &voxel_grid_gpu.local_voxel_write_list_);
            
        voxel_grid_gpu.stream_chunks_sphere(camera_controller.camera->position, -1, 45345345);
        window.draw(&voxel_grid_gpu, &camera);
        // window.draw(&torus, &camera);

        voxel_rasterizator.rasterize(torus, air_voxel_prifab, &voxel_grid_gpu.local_voxel_write_list_);
        ///////////////////////////////////////////////////////////////////////////////////////////////////

        torus.rotation.x += rotation_speed * delta_time;
        torus.rotation.y += rotation_speed * delta_time;
        torus.position = torus_origin_pos + torus_offset;

        ImGui::Begin("Torus");
        ImGui::SliderFloat("Position X", &torus_offset.x, -1000, 1000);
        ImGui::SliderFloat("Position Y", &torus_offset.y, -1000, 1000);
        ImGui::SliderFloat("Position Z", &torus_offset.z, -1000, 1000);
        ImGui::End();

        voxel_grid_debugger.dispay_debug_window();
        voxel_grid_debugger.display_build_cmd_window();
        voxel_grid_debugger.display_build_from_dirty_window();
        voxel_grid_debugger.display_chunk_eviction_window();
        voxel_grid_debugger.display_draw_pipline_window();
        voxel_grid_debugger.display_stream_chunks_pipeline_window();
        voxel_grid_debugger.display_hash_table_window();

        ui::end_frame();

        window.swap_buffers();
        engine.poll_events();
    }
}
