// main.cpp

#define ALLOW_LOCK_SSBO

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>
#include <cmath>
#include <fstream>

#include <functional>

#include "engine3d.h"

#include "vao.h"
#include "vbo.h"
#include "ebo.h"
#include "vertex_layout.h"
#include "program.h"
#include "camera.h"
#include "mesh.h"
#include "cube.h"
#include "window.h"
#include "fps_camera_controller.h"
#include "voxel_engine/chunk.h"
#include "voxel_engine/voxel_grid.h"
#include "imgui_layer.h"
#include "voxel_rastorizator.h"
#include "ui_elements/triangle_controller.h"
#include "triangle.h"
#include "vtk_mesh_loader.h"
#include "path_utils.h"
#include "voxel_rasterizator_gpu.h"
#include "shader_manager.h"
#include "voxel_grid_gpu.h"
#include "fstream"
#include "path_utils.h"
#include "glsl_preprocessor.h"

void create_voxels_box(
    glm::ivec3 box_origin, 
    glm::ivec3 box_dim, 
    glm::vec3 color_a, 
    glm::vec3 color_b, 
    double p_vis, 
    std::vector<glm::ivec3>& voxel_positions_out,
    std::vector<VoxelGridGPU::VoxelDataGPU> voxels_out
) {
    glm::vec3 box_center = glm::vec3(box_origin) + glm::vec3(box_dim) * glm::vec3(0.5);
    double max_lenght = glm::length(glm::vec3(box_dim)) / 2.0;
    
    voxel_positions_out.resize(box_dim.x * box_dim.y * box_dim.z);
    voxels_out.resize(box_dim.x * box_dim.y * box_dim.z);

    int id = 0;
    for (int x = 0; x < box_dim.x; x++)
        for (int y = 0; y < box_dim.y; y++)
            for (int z = 0; z < box_dim.z; z++) {
                uint32_t vis = 0;
                if (p_vis < (rand() % 10000) / 10000.0) {
                    vis = 1;
                }
                
                glm::vec3 current_pos = glm::vec3(x, y, z);
                double dis = glm::length(box_center - current_pos);
                double t = dis / max_lenght;
                glm::vec3 color = color_b + (color_a - color_b) * glm::vec3(t);
                glm::ivec3 icolor = color * glm::vec3(255);

                voxel_positions_out[id] = box_origin + glm::ivec3(x, y, z);
                // voxels[id].color = (icolor.x << 24) | (icolor.y << 16) | (icolor.z << 8) | 0xffu;
                voxels_out[id].color = (255 << 24) | (icolor.z << 16) | (icolor.y << 8) | icolor.x;
                voxels_out[id].type_vis_flags |= (vis & 0xFFu) << 16; // тип 1
                voxels_out[id].type_vis_flags |= (vis & 0xFFu) << 8; // не прозрачный
                id++;
            }
}

void create_occlusion_test(
    glm::ivec3 origin, 
    glm::ivec3 right_basis, 
    glm::ivec3 up_basis, 
    glm::ivec3 front_basis, 
    std::vector<glm::ivec3>& positions_out, 
    std::vector<VoxelGridGPU::VoxelDataGPU>& voxels_out
) {
    glm::imat3x3 transform = {right_basis, up_basis, front_basis};

    glm::ivec3 color = glm::ivec3(255, 255, 255);
    uint32_t count_changing_voxels = 8;
    uint32_t count_samples = 1 << count_changing_voxels;
    uint32_t side_size = count_samples >> (count_changing_voxels / 2);
    glm::ivec3 local_offsets[8] = {
        glm::ivec3(0, 0, 0),
        glm::ivec3(1, 0, 0),
        glm::ivec3(2, 0, 0),
        glm::ivec3(2, 1, 0),
        glm::ivec3(2, 2, 0),
        glm::ivec3(1, 2, 0),
        glm::ivec3(0, 2, 0),
        glm::ivec3(0, 1, 0)
    };

    voxels_out.reserve(count_samples * count_changing_voxels + count_samples);
    positions_out.reserve(count_samples * count_changing_voxels + count_samples);
    
    uint32_t id = 0;
    for (uint32_t x = 0; x < side_size; x++)
        for (uint32_t y = 0; y < side_size; y++) {
            for (uint32_t i = 0; i < count_changing_voxels; i++) {
                uint32_t vis = (id >> i) & 1u; // узнаём бит

                VoxelGridGPU::VoxelDataGPU side_voxel(1, 1, 0, color);
                
                glm::ivec3 local_offset = local_offsets[i];
                glm::ivec3 side_voxel_position = glm::ivec3(x * 4, y * 4, 0) + local_offset;

                glm::ivec3 side_voxel_position_in_new_basis = origin + transform * side_voxel_position;

                voxels_out.push_back(side_voxel);
                positions_out.push_back(side_voxel_position_in_new_basis);
            }
            
            VoxelGridGPU::VoxelDataGPU center_voxel(1, 1, 0, color);

            glm::ivec3 center_voxel_position = glm::ivec3(x * 4 + 1, y * 4 + 1, 1);
            glm::ivec3 center_voxel_position_in_new_basis = origin + transform * center_voxel_position;

            voxels_out.push_back(center_voxel);
            positions_out.push_back(center_voxel_position_in_new_basis);
            id++;
        }
}

void create_occlusion_test_box(glm::ivec3 origin, std::vector<glm::ivec3>& positions_out, std::vector<VoxelGridGPU::VoxelDataGPU>& voxels_out) {
    std::vector<glm::ivec3> positions_PZ;
    std::vector<glm::ivec3> positions_Z;
    std::vector<glm::ivec3> positions_PX;
    std::vector<glm::ivec3> positions_X;
    std::vector<glm::ivec3> positions_PY;
    std::vector<glm::ivec3> positions_Y;

    std::vector<VoxelGridGPU::VoxelDataGPU> voxels_PZ;
    std::vector<VoxelGridGPU::VoxelDataGPU> voxels_Z;
    std::vector<VoxelGridGPU::VoxelDataGPU> voxels_PX;
    std::vector<VoxelGridGPU::VoxelDataGPU> voxels_X;
    std::vector<VoxelGridGPU::VoxelDataGPU> voxels_PY;
    std::vector<VoxelGridGPU::VoxelDataGPU> voxels_Y;

    uint32_t count_changing_voxels = 8;
    uint32_t count_samples = 1 << count_changing_voxels;
    uint32_t side_count = count_samples >> (count_changing_voxels / 2);
    uint32_t side_size = side_count * 4;
    glm::ivec3 rt_origin = {side_size, side_size, side_size};

    create_occlusion_test({0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, positions_Z, voxels_Z);
    create_occlusion_test({0, 0, 0}, {0, 0, 1}, {0, 1, 0}, {1, 0, 0}, positions_PX, voxels_PX);
    create_occlusion_test({0, 0, 0}, {1, 0, 0}, {0, 0, 1}, {0, 1, 0}, positions_Y, voxels_Y);
    create_occlusion_test(rt_origin, {-1, 0, 0}, {0, -1, 0}, {0, 0, -1}, positions_PZ, voxels_PZ);
    create_occlusion_test(rt_origin, {0, 0, -1}, {0, -1, 0}, {-1, 0, 0}, positions_X, voxels_X);
    create_occlusion_test(rt_origin, {-1, 0, 0}, {0, 0, -1}, {0, -1, 0}, positions_PY, voxels_PY);

    std::vector<glm::ivec3> positions;
    positions.reserve((count_samples * count_changing_voxels + count_changing_voxels) * 6);

    std::vector<VoxelGridGPU::VoxelDataGPU> voxels;
    voxels.reserve((count_samples * count_changing_voxels + count_changing_voxels) * 6);

    positions.insert(positions.end(), positions_PX.begin(), positions_PX.end());
    positions.insert(positions.end(), positions_X.begin(), positions_X.end());
    positions.insert(positions.end(), positions_PY.begin(), positions_PY.end());
    positions.insert(positions.end(), positions_Y.begin(), positions_Y.end());
    positions.insert(positions.end(), positions_PZ.begin(), positions_PZ.end());
    positions.insert(positions.end(), positions_Z.begin(), positions_Z.end());

    voxels.insert(voxels.end(), voxels_PX.begin(), voxels_PX.end());
    voxels.insert(voxels.end(), voxels_X.begin(), voxels_X.end());
    voxels.insert(voxels.end(), voxels_PY.begin(), voxels_PY.end());
    voxels.insert(voxels.end(), voxels_Y.begin(), voxels_Y.end());
    voxels.insert(voxels.end(), voxels_PZ.begin(), voxels_PZ.end());
    voxels.insert(voxels.end(), voxels_Z.begin(), voxels_Z.end());

    positions_out = std::move(positions);
    voxels_out = std::move(voxels);
}

float clear_col[4] = {0.776470588f, 0.988235294f, 1.0f, 1.0f};
// float clear_col[4] = {0.0f, 0.0f, 0.0f, 1.0f};


int main() {
    Engine3D engine = Engine3D();
    Window window = Window(&engine, 1280, 720, "3D visualization");
    engine.set_window(&window);
    ui::init(window.window);
    
    window.disable_cursor();

    std::vector<std::filesystem::path> shaders_inlude_directories = {}; // Искать относительно executable_dir()
    ShaderManager shader_manager = ShaderManager(executable_dir(), shaders_inlude_directories);

    Camera camera;
    window.set_camera(&camera);

    FPSCameraController camera_controller = FPSCameraController(&camera);
    camera_controller.speed = 50;

    // GlslPreprocessor preprocessor;
    // std::string file_str = preprocessor.load(executable_dir() / "shaders" / "avoxel_grid_packed" / "mesh_alloc.glsl");
    
    // std::ofstream out("linked_file.glsl");
    // out << file_str;
    // out.close();
    

    // VoxelGrid voxel_grid = VoxelGrid({16, 16, 16}, 1.0f, {24, 6, 24});
    // float chunk_render_size = voxel_grid.chunk_size.x * voxel_grid.voxel_size;

    // VoxelRasterizatorGPU voxel_rastorizator(&voxel_grid, shader_manager);

    // VtkMeshLoader vtk_mesh_loader = VtkMeshLoader(vertex_layout);

    // MeshData model_mesh_data = vtk_mesh_loader.load_mesh((executable_dir() / "models" / "test_mesh.vtk").string());
    // Mesh model = Mesh(model_mesh_data.vertices, model_mesh_data.indices, &vertex_layout); 
    // model.position = {chunk_render_size * 0, chunk_render_size * 5, chunk_render_size * 0};
    // model.scale = glm::vec3(5.0f);

    float voxel_size = 1.0f;
    uint32_t chunk_size = 16u;
    VoxelGridGPU voxel_grid_gpu = VoxelGridGPU(
        glm::ivec3(chunk_size), // chunk_size
        glm::vec3(voxel_size), // voxel_size
        10'000, // count_active_chunks
        1'000'000, // max_quads
        4, // chunk_hash_table_size_factor
        4096, // count_evict_buckets
        5'000, // min_free_chunks
        0.2f, // tomb_fraction_to_rebuild
        chunk_size * voxel_size * 1, // eviction_bucket_shell_thickness
        10, // vb_page_size_order_of_two
        10, // ib_page_size_order_of_two
        1.0, // buddy_allocator_nodes_factor
        shader_manager
    );

    

    // std::vector<glm::ivec3> positions;
    // std::vector<VoxelGridGPU::VoxelDataGPU> voxels;
    // create_occlusion_test_box({0, 0, 0}, positions, voxels);
    // voxel_grid_gpu.apply_writes_to_world_from_cpu(positions, voxels);

    glm::vec3 prev_cam_pos = camera_controller.camera->position;

    std::function<void()> build_mesh_from_dirty_fn = [&](){
        voxel_grid_gpu.build_mesh_from_dirty(math_utils::BITS, math_utils::OFFSET);
    };

    std::function<void()> build_indirect_draw_commands_frustum_fn = [&]() {
        float aspect = window.get_fbuffer_aspect_ratio();
        glm::mat4 view_matrix = window.camera->get_view_matrix();
        glm::mat4 proj_matrix = window.camera->get_projection_matrix(aspect);
        glm::mat4 view_proj_matrix = proj_matrix * view_matrix;
        voxel_grid_gpu.build_indirect_draw_commands_frustum(view_proj_matrix, math_utils::BITS, math_utils::OFFSET);
    };

    std::function<void()> draw_indirect_fn = [&]() {
        float aspect = window.get_fbuffer_aspect_ratio();
        glm::mat4 view_matrix = window.camera->get_view_matrix();
        glm::mat4 proj_matrix = window.camera->get_projection_matrix(aspect);
        glm::mat4 view_proj_matrix = proj_matrix * view_matrix;

        voxel_grid_gpu.draw_indirect(voxel_grid_gpu.vao.id, glm::identity<glm::mat4>(), view_proj_matrix, window.camera->position);
    };

    bool voxel_grid_draw_streaming[3] = {false};
    std::function<void()> voxel_grid_draw_steps[3] = {build_mesh_from_dirty_fn, build_indirect_draw_commands_frustum_fn, draw_indirect_fn};
    std::string voxel_grid_draw_steps_names[3] = {"build_mesh_from_dirty()", "build_indirect_draw_commands_frustum_fn()", "draw_indirect()"};

    std::function<void()> mark_chunk_to_generate_fn = [&]() {
        int radius_chunks = 10;
        voxel_grid_gpu.mark_chunk_to_generate(window.camera->position, radius_chunks);
    };

    std::function<void()> generate_terrain_fn = [&]() {
        uint32_t seed = 45345345u;
        voxel_grid_gpu.prepare_dispatch_args(voxel_grid_gpu.dispatch_args, ValueDispatchArg(voxel_grid_gpu.vox_per_chunk), BufferDispatchArg(&voxel_grid_gpu.load_list_, 0u));
        voxel_grid_gpu.generate_terrain(voxel_grid_gpu.dispatch_args, seed);
    };

    std::function<void()> reset_load_list_counter_fn = [&]() {
        uint32_t load_count = voxel_grid_gpu.load_list_.read_scalar<uint32_t>(0);
        if (load_count == 0) return;

        voxel_grid_gpu.reset_load_list_counter();
    };

    bool voxel_grid_generation_streaming[3] = {false};
    std::function<void()> voxel_grid_generation_steps[3] = {mark_chunk_to_generate_fn, generate_terrain_fn, reset_load_list_counter_fn};
    std::string voxel_grid_generation_steps_names[3] = {"mark_chunk_to_generate()", "generate_terrain()", "reset_load_list_counter()"};

    std::filesystem::path state_dumps_dir = executable_dir() / "voxel_grid" / "state_dumps";
    std::filesystem::path dumps_dir = executable_dir() / "voxel_grid" / "dumps";
    std::filesystem::path base_dump_path = dumps_dir / "base_dump";
    std::filesystem::path dump_animations_path = dumps_dir / "dump_animations";
    std::filesystem::path frames_path_to_load = dump_animations_path / "to_load";

    int selected_frame_id = 0;
    int offset_verify_stack = 0;
    int count_elements_verify_stack = -1;
    int dirty_count_to_set = 0;
    bool use_verify_stack = false;

    float timer = 0;
    float lastFrame = 0;
    while(window.is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - lastFrame;
        timer += delta_time;
        lastFrame = currentFrame;   

        ui::begin_frame();
        ui::update_mouse_mode(&window);

        camera_controller.update(&window, delta_time);

        window.clear_color({clear_col[0], clear_col[1], clear_col[2], clear_col[3]});

        // voxel_grid.update(&window, &camera);
        // window.draw(&voxel_grid, &camera);
        // window.draw(&model, &camera);
        
        
        voxel_grid_gpu.stream_chunks_sphere(camera_controller.camera->position, 10, 45345345);
        // window.draw(&voxel_grid_gpu, &camera);

        ImGui::Begin("Debug");

        ImGui::TextUnformatted("Camera position");

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 220, 120, 255));
        ImGui::Text("x: %.3f", camera_controller.camera->position.x);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 255, 120, 255));
        ImGui::Text("y: %.3f", camera_controller.camera->position.y);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 180, 255, 255));
        ImGui::Text("z: %.3f", camera_controller.camera->position.z);
        ImGui::PopStyleColor();

        if (ImGui::Button("Print counters")) {
            voxel_grid_gpu.print_counters();
            std::cout << "-----------------------" << std::endl << std::endl;
        }
        if (ImGui::Button("Print dirty list")) {
            voxel_grid_gpu.print_dirty_list();
        }

        if (ImGui::Button("Print chunks hash table log")) {
            voxel_grid_gpu.print_chunks_hash_table_log();
        }

        if (ImGui::Button("Print vb free list")) {
            std::vector<VoxelGridGPU::AllocNode> vb_nodes(voxel_grid_gpu.count_vb_nodes_);
            voxel_grid_gpu.vb_nodes_.read_subdata(0, vb_nodes.data(), sizeof(VoxelGridGPU::AllocNode) * voxel_grid_gpu.count_vb_nodes_);

            std::vector<uint32_t> vb_heads(voxel_grid_gpu.vb_order_ + 1);
            voxel_grid_gpu.vb_heads_.read_subdata(0, vb_heads.data(), sizeof(uint32_t) * (voxel_grid_gpu.vb_order_ + 1));

            std::vector<uint32_t> vb_states(voxel_grid_gpu.count_vb_pages_);
            voxel_grid_gpu.vb_state_.read_subdata(0, vb_states.data(), sizeof(uint32_t) * voxel_grid_gpu.count_vb_pages_);

            for (uint32_t i = 0; i < voxel_grid_gpu.vb_order_ + 1; i++) {
                uint32_t order = i;
                std::cout << "======================ORDER " << order << "======================" << std::endl;
                uint32_t head_idx = vb_heads[order] >> voxel_grid_gpu.HEAD_TAG_BITS;
                uint32_t cur_node = head_idx != voxel_grid_gpu.INVALID_HEAD_IDX ? head_idx : voxel_grid_gpu.INVALID_ID;
                while (cur_node != voxel_grid_gpu.INVALID_ID) {
                    uint32_t page_id = vb_nodes[cur_node].page;
                    uint32_t kind = vb_states[page_id] & voxel_grid_gpu.ST_MASK;
                    uint32_t real_order = vb_states[page_id] >> voxel_grid_gpu.ST_MASK_BITS;
                    if (real_order == order) {
                        std::cout << page_id << " ";
                        if (kind == 0u) std::cout << "ST_FREE" << std::endl;
                    }
                    cur_node = vb_nodes[cur_node].next;
                }
                
                std::cout << std::endl;
            }
        }

        if (ImGui::Button("Print ib free list")) {
            std::vector<VoxelGridGPU::AllocNode> ib_nodes(voxel_grid_gpu.count_ib_nodes_);
            voxel_grid_gpu.ib_nodes_.read_subdata(0, ib_nodes.data(), sizeof(VoxelGridGPU::AllocNode) * voxel_grid_gpu.count_ib_nodes_);

            std::vector<uint32_t> ib_heads(voxel_grid_gpu.ib_order_ + 1);
            voxel_grid_gpu.ib_heads_.read_subdata(0, ib_heads.data(), sizeof(uint32_t) * (voxel_grid_gpu.ib_order_ + 1));

            std::vector<uint32_t> ib_states(voxel_grid_gpu.count_ib_pages_);
            voxel_grid_gpu.ib_state_.read_subdata(0, ib_states.data(), sizeof(uint32_t) * voxel_grid_gpu.count_ib_pages_);

            for (uint32_t i = 0; i < voxel_grid_gpu.ib_order_ + 1; i++) {
                uint32_t order = i;
                std::cout << "======================ORDER " << order << "======================" << std::endl;
                uint32_t head_idx = ib_heads[order] >> voxel_grid_gpu.HEAD_TAG_BITS;
                uint32_t cur_node = head_idx != voxel_grid_gpu.INVALID_HEAD_IDX ? head_idx : voxel_grid_gpu.INVALID_ID;
                while (cur_node != voxel_grid_gpu.INVALID_ID) {
                    uint32_t page_id = ib_nodes[cur_node].page;
                    uint32_t kind = ib_states[page_id] & voxel_grid_gpu.ST_MASK;
                    uint32_t real_order = ib_states[page_id] >> voxel_grid_gpu.ST_MASK_BITS;
                    if (real_order == order) {
                        std::cout << page_id << " ";
                        if (kind == 0u) std::cout << "ST_FREE" << std::endl;
                        // else if (kind == 1u) std::cout << "ST_ALLOC" << std::endl;
                        // else if (kind == 2u) std::cout << "ST_MERGED" << std::endl;
                        // else if (kind == 3u) std::cout << "ST_MERGING" << std::endl;
                        // else if (kind == 4u) std::cout << "ST_READY" << std::endl;
                        // else if (kind == 5u) std::cout << "ST_CONCEDED" << std::endl;
                    }
                    cur_node = ib_nodes[cur_node].next;
                }
                
                std::cout << std::endl;
            }
        }

        // if (ImGui::Button("Print stream_generate_terrain debug counters")) {
        //     uint32_t stats[2];
        //     voxel_grid_gpu.stream_generate_debug_counters_.read_subdata(0, stats, sizeof(uint32_t) * 2);

        //     std::cout << "stream_generate_terrain debug counters" << std::endl;
        //     std::cout << "neighbor_lookup_invalid: " << stats[0] << std::endl;
        //     std::cout << "neighbor_marked: " << stats[1] << std::endl;

        //     std::cout << "-----------------------" << std::endl << std::endl;
        // }

        // if (ImGui::Button("mark_chunk_to_generate()")) {
        //     voxel_grid_gpu.mark_chunk_to_generate(camera_controller.camera->position, 10);
        //     std::cout << "mark_chunk_to_generate()" << std::endl;
        // }

        // if (ImGui::Button("generate_terrain()")) {
        //     uint32_t load_count = 0;
        //     voxel_grid_gpu.stream_counters_.read_subdata(0, &load_count, sizeof(uint32_t));
        //     if (load_count != 0) {
        //         voxel_grid_gpu.generate_terrain(45345345, load_count);
        //         voxel_grid_gpu.reset_load_list_counter();
        //         // voxel_grid_gpu.mark_all_used_chunks_as_dirty();
        //         std::cout << "generate_terrain()" << std::endl;
        //     } else {
        //         std::cout << "load_count == 0" << std::endl;
        //     }
            
        // }

        // if (ImGui::Button("draw()")) {
        //     std::cout << "draw()" << std::endl;
        //     window.draw(&voxel_grid_gpu, &camera);
        // }

        if (ImGui::Button("Save VB states dump")) {
            std::vector<uint32_t> vb_state(voxel_grid_gpu.count_vb_pages_);
            voxel_grid_gpu.vb_state_.read_subdata(0, vb_state.data(), sizeof(uint32_t) * voxel_grid_gpu.count_vb_pages_);

            std::filesystem::path vb_states_dir = state_dumps_dir / "vb";
            std::filesystem::create_directories(vb_states_dir);
            
            std::string dump_file_name = "vb_dump" + math_utils::get_current_date_time() + ".csv"; 
            
            std::replace(dump_file_name.begin(), dump_file_name.end(), ':', '-');
            std::replace(dump_file_name.begin(), dump_file_name.end(), ' ', '_');

            std::ofstream vb_out(vb_states_dir / dump_file_name);

            vb_out << "page_id,kind,order\n";
            
            for (uint32_t page_id = 0; page_id < voxel_grid_gpu.count_vb_pages_; page_id++) {
                uint32_t kind = vb_state[page_id] & voxel_grid_gpu.ST_MASK;
                uint32_t order = vb_state[page_id] >> voxel_grid_gpu.ST_MASK_BITS;
                vb_out << page_id << "," << kind << "," << order << "\n";
            }
            vb_out.close();
        }

        if (ImGui::Button("Save ib states dump")) {
            std::vector<uint32_t> ib_state(voxel_grid_gpu.count_ib_pages_);
            voxel_grid_gpu.ib_state_.read_subdata(0, ib_state.data(), sizeof(uint32_t) * voxel_grid_gpu.count_ib_pages_);

            std::filesystem::path ib_states_dir = state_dumps_dir / "ib";
            std::filesystem::create_directories(ib_states_dir);
            
            std::string dump_file_name = "ib_dump" + math_utils::get_current_date_time() + ".csv"; 
            
            std::replace(dump_file_name.begin(), dump_file_name.end(), ':', '-');
            std::replace(dump_file_name.begin(), dump_file_name.end(), ' ', '_');
            
            std::ofstream ib_out(ib_states_dir / dump_file_name);

            ib_out << "page_id,kind,order\n";
            
            for (uint32_t page_id = 0; page_id < voxel_grid_gpu.count_ib_pages_; page_id++) {
                uint32_t kind = ib_state[page_id] & voxel_grid_gpu.ST_MASK;
                uint32_t order = ib_state[page_id] >> voxel_grid_gpu.ST_MASK_BITS;
                ib_out << page_id << "," << kind << "," << order << "\n";
            }
            ib_out.close();
        }

        // if (ImGui::Button("Save base dump")) {
        //     voxel_grid_gpu.save_verify_mesh_buffers_dumps(base_dump_path);

        //     std::set<uint32_t> vb_limbo = voxel_grid_gpu.find_limbo_pages(
        //         voxel_grid_gpu.vb_heads_, 
        //         voxel_grid_gpu.vb_state_, 
        //         voxel_grid_gpu.vb_next_, 
        //         voxel_grid_gpu.vb_order_,
        //         voxel_grid_gpu.count_vb_pages_
        //     );

        //     std::set<uint32_t> ib_limbo = voxel_grid_gpu.find_limbo_pages(
        //         voxel_grid_gpu.ib_heads_, 
        //         voxel_grid_gpu.ib_state_, 
        //         voxel_grid_gpu.ib_next_, 
        //         voxel_grid_gpu.ib_order_,
        //         voxel_grid_gpu.count_ib_pages_
        //     );

        //     std::ofstream vb_out(base_dump_path / "vb_limbo.txt");
        //     for (uint32_t page_id : vb_limbo) { vb_out << page_id << "\n"; }
        //     vb_out.close();

        //     std::ofstream ib_out(base_dump_path / "ib_limbo.txt");
        //     for (uint32_t page_id : ib_limbo) { ib_out << page_id << "\n"; }
        //     ib_out.close();
        // }

        // if (ImGui::Button("Save verify_mesh_allocation dump-animation")) {
        //     voxel_grid_gpu.load_verify_mesh_buffers_dumps(base_dump_path);

        //     std::string folder_name = math_utils::get_current_date_time();
        //     std::filesystem::path dir = dump_animations_path / folder_name;

        //     uint32_t dirty_count = voxel_grid_gpu.frame_counters_.read_scalar<uint32_t>(sizeof(uint32_t));
        //     for (uint32_t cur_dirty_count = 0; cur_dirty_count <= dirty_count; cur_dirty_count++) {
        //         voxel_grid_gpu.load_verify_mesh_buffers_dumps(base_dump_path);

        //         voxel_grid_gpu.frame_counters_.update_subdata(sizeof(uint32_t), &cur_dirty_count, sizeof(uint32_t));
        //         voxel_grid_gpu.verify_mesh_allocation(cur_dirty_count);

        //         std::string frame_name = "frame_" + std::to_string(cur_dirty_count);
        //         std::filesystem::path frame_dir = dir / frame_name;
        //         voxel_grid_gpu.save_verify_mesh_buffers_dumps(frame_dir);

        //         std::set<uint32_t> vb_limbo = voxel_grid_gpu.find_limbo_pages(
        //             voxel_grid_gpu.vb_heads_, 
        //             voxel_grid_gpu.vb_state_, 
        //             voxel_grid_gpu.vb_next_, 
        //             voxel_grid_gpu.vb_order_,
        //             voxel_grid_gpu.count_vb_pages_
        //         );

        //         std::set<uint32_t> ib_limbo = voxel_grid_gpu.find_limbo_pages(
        //             voxel_grid_gpu.ib_heads_, 
        //             voxel_grid_gpu.ib_state_, 
        //             voxel_grid_gpu.ib_next_, 
        //             voxel_grid_gpu.ib_order_,
        //             voxel_grid_gpu.count_ib_pages_
        //         );

        //         std::ofstream vb_out(frame_dir / "vb_limbo.txt");
        //         for (uint32_t page_id : vb_limbo) { vb_out << page_id << "\n"; }
        //         vb_out.close();

        //         std::ofstream ib_out(frame_dir / "ib_limbo.txt");
        //         for (uint32_t page_id : ib_limbo) { ib_out << page_id << "\n"; }
        //         ib_out.close();
        //     }
        // }
        ImGui::Separator();

        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("dirty_count", &dirty_count_to_set);
        ImGui::SameLine();
        if (ImGui::Button("Set dirty count")) {
            voxel_grid_gpu.mesh_buffers_status_.update_subdata_fill<uint32_t>(sizeof(uint32_t) * 1, (uint32_t)(dirty_count_to_set), sizeof(uint32_t), shader_manager);
            std::cout << "Dirty count has been set to " << dirty_count_to_set << "." << std::endl;
        }

        ImGui::Separator();

        if (ImGui::Button("Checkout to base")) {
            if (std::filesystem::exists(base_dump_path)) {
                voxel_grid_gpu.load_verify_mesh_buffers_dumps(base_dump_path);
                std::cout << "Moved to base " << std::endl;
            } else {
                std::cout << "Directory: " << std::endl;
                std::cout << base_dump_path << std::endl;
                std::cout << "Does not exist!" << std::endl;
            }
            
        }

        if (ImGui::Button("Checkout to frame")) {
            std::filesystem::path dumps_path = frames_path_to_load / ("frame_" + std::to_string(selected_frame_id));
            if (std::filesystem::exists(dumps_path)) {
                voxel_grid_gpu.load_verify_mesh_buffers_dumps(dumps_path);
                std::cout << "Moved to frame " << selected_frame_id << std::endl;
            } else {
                std::cout << "Directory: " << std::endl;
                std::cout << dumps_path << std::endl;
                std::cout << "Does not exist!" << std::endl;
            }
            
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("Frame number", &selected_frame_id);

        ImGui::Separator();
        
        bool use_verify_stack_local = use_verify_stack;
        ImGui::Checkbox("Enable verify stack", &use_verify_stack_local);

        if (use_verify_stack != use_verify_stack_local) {
            use_verify_stack = use_verify_stack_local;
            
            voxel_grid_gpu.verify_debug_stack_.update_subdata_fill<uint32_t>(0, (use_verify_stack ? 1u : 0u), sizeof(uint32_t), shader_manager);

            if (use_verify_stack)
                std::cout << "Verify stack is enable." << std::endl;
            else 
                std::cout << "Verify stack is disable." << std::endl;
        }

        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("Offset", &offset_verify_stack);
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("Count elements", &count_elements_verify_stack);
        if (ImGui::Button("Print verify debug stack")) {
            voxel_grid_gpu.print_verify_debug_stack(offset_verify_stack, count_elements_verify_stack);
        }
        ImGui::Separator();
        // if (ImGui::CollapsingHeader("Limbo", 
        // ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding)) {
        //     ImGui::Text("States vs headers limbo");
        //     if (ImGui::Button("vb")) {
        //         std::set<uint32_t> vb_limbo = voxel_grid_gpu.find_limbo_pages(
        //             voxel_grid_gpu.vb_heads_, 
        //             voxel_grid_gpu.vb_state_, 
        //             voxel_grid_gpu.vb_next_, 
        //             voxel_grid_gpu.vb_order_,
        //             voxel_grid_gpu.count_vb_pages_
        //         );

        //         std::cout << "States vs heads limbo: VB" << std::endl;
        //         if (vb_limbo.size() > 0) {
        //             for (uint32_t page_id : vb_limbo) {
        //                 std::cout << "PAGE_ID: " << page_id << std::endl; 
        //             }
        //         } else {
        //             std::cout << "There is no VB limbo." << std::endl;
        //         }
        //         std::cout << std::endl;
        //     }

        //     ImGui::SameLine();

        //     if (ImGui::Button("ib")) {
        //         std::set<uint32_t> ib_limbo = voxel_grid_gpu.find_limbo_pages(
        //             voxel_grid_gpu.ib_heads_, 
        //             voxel_grid_gpu.ib_state_, 
        //             voxel_grid_gpu.ib_next_, 
        //             voxel_grid_gpu.ib_order_,
        //             voxel_grid_gpu.count_ib_pages_
        //         );

        //         std::cout << "States vs heads limbo: IB" << std::endl;
        //         if (ib_limbo.size() > 0) {
        //             for (uint32_t page_id : ib_limbo) {
        //                 std::cout << "PAGE_ID: " << page_id << std::endl; 
        //             }
        //         } else {
        //             std::cout << "There is no IB limbo." << std::endl;
        //         }
        //         std::cout << std::endl;
        //     }
        // }
        

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Dirty list data", 
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding)) {
            ImGui::Text("Mesh alloc");

            bool vb_button = ImGui::Button("Print VB");
            ImGui::SameLine();
            bool ib_button = ImGui::Button("Print IB");

            std::vector<VoxelGridGPU::ChunkMeshAlloc> alloc_meta(voxel_grid_gpu.count_active_chunks);
            std::vector<uint32_t> dirty_list;
            uint32_t dirty_count;

            if (vb_button || ib_button) {
                voxel_grid_gpu.chunk_mesh_alloc_.read_subdata(0, alloc_meta.data(), sizeof(VoxelGridGPU::ChunkMeshAlloc) * voxel_grid_gpu.count_active_chunks);
                voxel_grid_gpu.mesh_buffers_status_.read_subdata(sizeof(uint32_t), &dirty_count, sizeof(uint32_t));
                dirty_list.resize(dirty_count);
                voxel_grid_gpu.dirty_list_.read_subdata(0, dirty_list.data(), sizeof(uint32_t) * dirty_count);
            }
            
            if (vb_button) {
                if (dirty_count > 0) {
                    std::cout << "VB mesh allocs of dirty list:" << std::endl;
                    uint32_t count_alloc_pages = 0, count_alloc_states = 0; 
                    for (uint32_t dirty_idx = 0; dirty_idx < dirty_count; dirty_idx++) {
                        uint32_t chunk_id = dirty_list[dirty_idx];
                        if (alloc_meta[chunk_id].v_startPage == 0xFFFFFFFFu) continue;
                        count_alloc_pages += 1u << alloc_meta[chunk_id].v_order;
                        count_alloc_states++;
                    }

                    std::cout << "ST_ALLOC:     " << "count states = " << count_alloc_states << "   count pages = " << count_alloc_pages << std::endl;
                    std::cout << std::endl;

                    for (uint32_t dirty_idx = 0; dirty_idx < dirty_count; dirty_idx++) {
                        uint32_t chunk_id = dirty_list[dirty_idx];

                        if (alloc_meta[chunk_id].v_startPage == 0xFFFFFFFFu) continue;
                        
                        std::cout << std::left << std::setw(9 + 8) << ("DIRTY_ID " + std::to_string(dirty_idx))
                                << std::left << std::setw(5) << "  |"
                                << std::left << std::setw(9 + 8) << ("CHUNK_ID " + std::to_string(chunk_id) + ":")
                                << std::right << std::setw(13 + 5) << "start page = "
                                << std::right << std::setw(6 + 5) << alloc_meta[chunk_id].v_startPage
                                << std::right << std::setw(8 + 5) << "order = "
                                << std::right << std::setw(3 + 5) << alloc_meta[chunk_id].v_order
                                << std::endl;
                    }
                    std::cout << std::endl;
                } else {
                    std::cout << "===DIRTY LIST IS EMPTY===" << std::endl;
                    std::cout << std::endl;
                }
            }

            if (ib_button) {
                if (dirty_count > 0) {
                    std::cout << "IB mesh allocs of dirty list:" << std::endl;
                    uint32_t count_alloc_pages = 0, count_alloc_states = 0; 
                    for (uint32_t dirty_idx = 0; dirty_idx < dirty_count; dirty_idx++) {
                        uint32_t chunk_id = dirty_list[dirty_idx];
                        if (alloc_meta[chunk_id].i_startPage == 0xFFFFFFFFu) continue;
                        count_alloc_pages += 1u << alloc_meta[chunk_id].i_order;
                        count_alloc_states++;
                    }

                    std::cout << "ST_ALLOC:     " << "count states = " << count_alloc_states << "   count pages = " << count_alloc_pages << std::endl;
                    std::cout << std::endl;

                    for (uint32_t dirty_idx = 0; dirty_idx < dirty_count; dirty_idx++) {
                        uint32_t chunk_id = dirty_list[dirty_idx];

                        if (alloc_meta[chunk_id].i_startPage == 0xFFFFFFFFu) continue;
                        
                        std::cout << std::left << std::setw(9 + 8) << ("DIRTY_ID " + std::to_string(dirty_idx))
                                << std::left << std::setw(9 + 8) << "        |"
                                << std::left << std::setw(9 + 8) << ("CHUNK_ID " + std::to_string(chunk_id) + ":")
                                << std::right << std::setw(13 + 5) << "start page = "
                                << std::right << std::setw(6 + 5) << alloc_meta[chunk_id].i_startPage
                                << std::right << std::setw(8 + 5) << "order = "
                                << std::right << std::setw(3 + 5) << alloc_meta[chunk_id].i_order
                                << std::endl;
                    }
                    std::cout << std::endl;
                } else {
                    std::cout << "===DIRTY LIST IS EMPTY===" << std::endl;
                    std::cout << std::endl;
                }
            }

            ImGui::Separator();

            // ImGui::Checkbox("streaming##1", &stream1);
            // ImGui::SameLine();
            // if (ImGui::Button("Run once##1")) { /* ... */ }
            // ImGui::Spacing();
        }

        ImGui::End();

        ImGui::Begin("Build mesh from dirty pipeline");
        if (ImGui::Button("Run all pipeline")) {
            voxel_grid_gpu.build_mesh_from_dirty(math_utils::BITS, math_utils::OFFSET);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("Pipeline steps");
        ImGui::Separator();

        if (ImGui::Button("reset_global_mesh_counters()")) {
            // voxel_grid_gpu.reset_global_mesh_counters();
        }

        if (ImGui::Button("mesh_reset()")) {
            voxel_grid_gpu.prepare_dispatch_args(voxel_grid_gpu.dispatch_args, BufferDispatchArg(&voxel_grid_gpu.mesh_buffers_status_, 1u));
            voxel_grid_gpu.mesh_reset(voxel_grid_gpu.dispatch_args);
        }

        if (ImGui::Button("mesh_count()")) {
            uint32_t vox_per_chunk = (uint32_t)(voxel_grid_gpu.chunk_size.x * voxel_grid_gpu.chunk_size.y * voxel_grid_gpu.chunk_size.z);
            voxel_grid_gpu.prepare_dispatch_args(
                voxel_grid_gpu.dispatch_args, 
                ValueDispatchArg(vox_per_chunk), 
                BufferDispatchArg(&voxel_grid_gpu.mesh_buffers_status_, 1u)
            );
            voxel_grid_gpu.mesh_count(voxel_grid_gpu.dispatch_args, math_utils::BITS, math_utils::OFFSET);
        }

        if (ImGui::Button("mesh_alloc()")) {
            voxel_grid_gpu.prepare_dispatch_args(voxel_grid_gpu.dispatch_args, BufferDispatchArg(&voxel_grid_gpu.mesh_buffers_status_, 1u));
            voxel_grid_gpu.mesh_alloc(voxel_grid_gpu.dispatch_args);
        }

        if (ImGui::Button("verify_mesh_allocation()")) {
            voxel_grid_gpu.prepare_dispatch_args(voxel_grid_gpu.dispatch_args, BufferDispatchArg(&voxel_grid_gpu.mesh_buffers_status_, 1u));
            voxel_grid_gpu.verify_mesh_allocation(voxel_grid_gpu.mesh_buffers_status_);
        }

        if (ImGui::Button("return_free_alloc_nodes()")) {
            voxel_grid_gpu.prepare_return_free_alloc_nodes(voxel_grid_gpu.mesh_buffers_status_);
            voxel_grid_gpu.return_free_alloc_nodes(voxel_grid_gpu.mesh_buffers_status_);
        }

        if (ImGui::Button("mesh_emit()")) {
            uint32_t vox_per_chunk = (uint32_t)(voxel_grid_gpu.chunk_size.x * voxel_grid_gpu.chunk_size.y * voxel_grid_gpu.chunk_size.z);
            voxel_grid_gpu.prepare_dispatch_args(
                voxel_grid_gpu.dispatch_args, 
                ValueDispatchArg(vox_per_chunk), 
                BufferDispatchArg(&voxel_grid_gpu.mesh_buffers_status_, 1u)
            );
            voxel_grid_gpu.mesh_emit(voxel_grid_gpu.dispatch_args, math_utils::BITS, math_utils::OFFSET);
        }

        if (ImGui::Button("mesh_finalize()")) {
            voxel_grid_gpu.prepare_dispatch_args(voxel_grid_gpu.dispatch_args, BufferDispatchArg(&voxel_grid_gpu.mesh_buffers_status_, 1u));
            voxel_grid_gpu.mesh_finalize(voxel_grid_gpu.dispatch_args);
        }

        if (ImGui::Button("reset_dirty_count()")) {
            voxel_grid_gpu.reset_dirty_count();
        }
        ImGui::End();

        ImGui::Begin("Build draw commands pipeline");
        if (ImGui::Button("Run all pipeline")) {
            float aspect = window.get_fbuffer_aspect_ratio();
            glm::mat4 view_matrix = window.camera->get_view_matrix();
            glm::mat4 proj_matrix = window.camera->get_projection_matrix(aspect);
            glm::mat4 view_proj_matrix = proj_matrix * view_matrix;
            voxel_grid_gpu.build_indirect_draw_commands_frustum(view_proj_matrix, math_utils::BITS, math_utils::OFFSET);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("Pipeline steps");
        ImGui::Separator();

        if (ImGui::Button("reset_cmd_count()")) {
            voxel_grid_gpu.mesh_buffers_status_.update_subdata_fill<uint32_t>(2, 0u, sizeof(uint32_t), shader_manager);
        }

        if (ImGui::Button("build_draw_commands()")) {
            float aspect = window.get_fbuffer_aspect_ratio();
            glm::mat4 view_matrix = window.camera->get_view_matrix();
            glm::mat4 proj_matrix = window.camera->get_projection_matrix(aspect);
            glm::mat4 view_proj_matrix = proj_matrix * view_matrix;
            voxel_grid_gpu.build_draw_commands(view_proj_matrix, math_utils::BITS, math_utils::OFFSET);
        }
        ImGui::End();

        ImGui::Begin("Voxel grid draw pipeline");
        if (ImGui::BeginTable("pipeline_table", 3, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableSetupColumn("Step");
            ImGui::TableSetupColumn("Streaming", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Run", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableHeadersRow();

            for (uint32_t i = 0; i < 3; i++) {
                ImGui::TableNextRow(); // ----
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(voxel_grid_draw_steps_names[i].c_str());

                ImGui::TableNextColumn();
                ImGui::PushID(i);

                ImGui::Checkbox("streaming", &voxel_grid_draw_streaming[i]);
                
                ImGui::TableNextColumn();
                if (ImGui::Button("Run once")) {
                    voxel_grid_draw_steps[i]();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        for (uint32_t i = 0; i < 3; i++) {
            if (voxel_grid_draw_streaming[i]) {
                voxel_grid_draw_steps[i]();
            }
        }
        ImGui::End();

        ImGui::Begin("Chunk enviction");

        if (ImGui::Button("Run all pipeline")) voxel_grid_gpu.ensure_free_chunks_gpu(window.camera->position, math_utils::BITS, math_utils::OFFSET);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("Debug");
        ImGui::Separator();

        if (ImGui::Button("print eviction log")) voxel_grid_gpu.print_eviction_log(window.camera->position);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("Pipeline steps");
        ImGui::Separator();

        if (ImGui::Button("reset_heads()")) voxel_grid_gpu.reset_heads();
        if (ImGui::Button("build_bucket_lists()")) voxel_grid_gpu.build_bucket_lists(window.camera->position);
        if (ImGui::Button("evict_lowpriority_chunks()")) {
            voxel_grid_gpu.prepare_evict_lowpriority_chunks(voxel_grid_gpu.dispatch_args);
            voxel_grid_gpu.evict_lowpriority_chunks(voxel_grid_gpu.dispatch_args);
        }
        if (ImGui::Button("free_evicted_chunks_mesh()"))  {
            voxel_grid_gpu.prepare_evict_lowpriority_chunks(voxel_grid_gpu.dispatch_args);
            voxel_grid_gpu.free_evicted_chunks_mesh(voxel_grid_gpu.dispatch_args);
        }
        if (ImGui::Button("return_free_alloc_nodes()")) {
            voxel_grid_gpu.prepare_return_free_alloc_nodes(voxel_grid_gpu.dispatch_args);
            voxel_grid_gpu.return_free_alloc_nodes(voxel_grid_gpu.dispatch_args);
        }

        ImGui::End();

        ImGui::Begin("Steam chunks pipeline");
        if (ImGui::BeginTable("pipeline_table", 3, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableSetupColumn("Step");
            ImGui::TableSetupColumn("Streaming", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Run", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableHeadersRow();

            for (uint32_t i = 0; i < 3; i++) {
                ImGui::TableNextRow(); // ----
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(voxel_grid_generation_steps_names[i].c_str());

                ImGui::TableNextColumn();
                ImGui::PushID(i);

                ImGui::Checkbox("streaming", &voxel_grid_generation_streaming[i]);
                
                ImGui::TableNextColumn();
                if (ImGui::Button("Run once")) {
                    voxel_grid_generation_steps[i]();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        for (uint32_t i = 0; i < 3; i++) {
            if (voxel_grid_generation_streaming[i]) {
                voxel_grid_generation_steps[i]();
            }
        }
        ImGui::End();

        ui::end_frame();

        window.swap_buffers();
        engine.poll_events();

        prev_cam_pos = camera_controller.camera->position;
    }
    
    ui::shutdown();
}
