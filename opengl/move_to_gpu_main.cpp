// main.cpp
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>
#include <cmath>

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

                VoxelGridGPU::VoxelDataGPU side_voxel = {0};
                side_voxel.color = (255 << 24) | (color.z << 16) | (color.y << 8) | color.x;
                side_voxel.type_vis_flags |= (vis & 0xFFu) << 16; // тип 1
                side_voxel.type_vis_flags |= (vis & 0xFFu) << 8; // не прозрачный
                
                glm::ivec3 local_offset = local_offsets[i];
                glm::ivec3 side_voxel_position = glm::ivec3(x * 4, y * 4, 0) + local_offset;

                glm::ivec3 side_voxel_position_in_new_basis = origin + transform * side_voxel_position;

                voxels_out.push_back(side_voxel);
                positions_out.push_back(side_voxel_position_in_new_basis);
            }
            
            VoxelGridGPU::VoxelDataGPU center_voxel = {0};
            center_voxel.color = (255 << 24) | (color.z << 16) | (color.y << 8) | color.x;
            center_voxel.type_vis_flags |= (1u & 0xFFu) << 16; // тип 1
            center_voxel.type_vis_flags |= (1u & 0xFFu) << 8; // не прозрачный

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

struct Frame {
    uint64_t timestamp_ns = 0;
    uint32_t flags = 0;        // 1=rgb, 2=intensity
    std::vector<PointXYZRGBI> points;
};

struct IndexEntry {
    uint64_t frame_id = 0;
    uint64_t timestamp_ns = 0;
    std::filesystem::path filename;
    uint32_t point_count = 0;
};


static Frame read_frame_bin(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Failed to open: " + path.string());

    Frame f{};
    uint32_t count = 0;

    in.read(reinterpret_cast<char*>(&f.timestamp_ns), sizeof(uint64_t));
    in.read(reinterpret_cast<char*>(&count), sizeof(uint32_t));
    in.read(reinterpret_cast<char*>(&f.flags), sizeof(uint32_t));
    if (!in) throw std::runtime_error("Bad header in: " + path.string());

    const bool has_rgb = (f.flags & 1u) != 0;
    const bool has_intensity = (f.flags & 2u) != 0;

    size_t bpp = 3 * sizeof(float);
    if (has_rgb) bpp += 3 * sizeof(uint8_t);
    if (has_intensity) bpp += sizeof(float);

    std::vector<uint8_t> buf(size_t(count) * bpp);
    in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    if (!in) throw std::runtime_error("Unexpected EOF in: " + path.string());

    f.points.resize(count);

    const uint8_t* p = buf.data();
    for (uint32_t i = 0; i < count; ++i) {
        std::memcpy(&f.points[i].x, p, 4); p += 4;
        std::memcpy(&f.points[i].y, p, 4); p += 4;
        std::memcpy(&f.points[i].z, p, 4); p += 4;

        f.points[i].r = f.points[i].g = f.points[i].b = 255;
        f.points[i].intensity = 0.0f;

        if (has_rgb) {
            f.points[i].r = *p++;
            f.points[i].g = *p++;
            f.points[i].b = *p++;
        }
        if (has_intensity) {
            std::memcpy(&f.points[i].intensity, p, 4); p += 4;
        }
    }

    return f;
}

// Very small CSV parser for: frame_id,timestamp_ns,filename,point_count
// Skips empty/bad lines. Supports optional header row.
static std::vector<IndexEntry> read_index_csv(const std::filesystem::path& index_path)
{
    std::ifstream in(index_path);
    if (!in) throw std::runtime_error("Failed to open: " + index_path.string());

    std::vector<IndexEntry> entries;
    std::string line;

    while (std::getline(in, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string a, b, c, d;

        if (!std::getline(ss, a, ',')) continue;
        if (!std::getline(ss, b, ',')) continue;
        if (!std::getline(ss, c, ',')) continue;
        if (!std::getline(ss, d, ',')) continue;

        // If it's a header (non-numeric first field), skip it
        auto is_uint = [](const std::string& s) {
            if (s.empty()) return false;
            for (char ch : s) if (ch < '0' || ch > '9') return false;
            return true;
        };
        if (!is_uint(a) || !is_uint(b) || !is_uint(d)) continue;

        IndexEntry e;
        e.frame_id = std::stoull(a);
        e.timestamp_ns = std::stoull(b);
        e.filename = c;
        e.point_count = static_cast<uint32_t>(std::stoul(d));
        entries.push_back(std::move(e));
    }

    std::sort(entries.begin(), entries.end(),
              [](const IndexEntry& x, const IndexEntry& y) { return x.timestamp_ns < y.timestamp_ns; });

    return entries;
}



float clear_col[4] = {0.776470588f, 0.988235294f, 1.0f, 1.0f};
// float clear_col[4] = {0.0f, 0.0f, 0.0f, 1.0f};


std::vector<LineInstance> get_arrow(glm::vec3 p0, glm::vec3 p1) {
    LineInstance middle{p0, p1};

    glm::vec3 d = p1 - p0;
    float len = glm::length(d);
    if (len < 1e-6f) return { middle };

    glm::vec3 dir = d / len; // arrow direction (unit)

    // --- constant arrowhead size (world units) ---
    const float head_len   = 0.3f; // distance from tip back to where head starts
    const float head_width = 0.2f; // half-width of the V

    // Put head base head_len behind the tip, but don't go past p0 for very short arrows
    float back = std::min(head_len, 0.9f * len);
    glm::vec3 head_base = p1 - dir * back;

    // Choose a reference axis not parallel to dir
    glm::vec3 ref(0, 1, 0);
    if (std::abs(glm::dot(dir, ref)) > 0.99f)
        ref = glm::vec3(1, 0, 0);

    glm::vec3 side = glm::normalize(glm::cross(dir, ref));

    LineInstance left;
    left.p0 = head_base + side * head_width;
    left.p1 = p1;

    LineInstance right;
    right.p0 = head_base - side * head_width;
    right.p1 = p1;

    // return { left, middle, right };
    return { left, middle, right };
}

void draw_f(Window* window, 
    Camera* camera, 
    VoxelGrid* voxel_grid, 
    NonholonomicAStar* astar,
    PlainAstarData plain_a_star_path,
    const NonholonomicPos& start_pos, const NonholonomicPos& end_pos, 
    int size) {
    glm::ivec3 camera_voxel_pos = glm::ivec3(glm::floor(camera->position));

    voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor){

        glm::ivec3 left_top_pos = (glm::ivec3)start_pos.pos -  glm::ivec3(size/2.0f, 0, size/2.0f);

        Voxel paint_voxel;

        float max_f = 0;
        for (int x = 0; x < size; x++)
            for (int z = 0; z < size; z++) {
                NonholonomicPos new_pos;
                new_pos.pos = (glm::vec3)left_top_pos + glm::vec3(x, 0, z);

                float f = astar->get_nonholonomic_f(new_pos, end_pos, new_pos, plain_a_star_path);

                if (f > max_f)
                    max_f = f;
            }

        for (int x = 0; x < size; x++)
            for (int z = 0; z < size; z++) {
                NonholonomicPos new_pos;
                new_pos.pos = (glm::vec3)left_top_pos + glm::vec3(x, 0, z);

                float f = astar->get_nonholonomic_f(new_pos, end_pos, new_pos, plain_a_star_path);

                float color_value = f / max_f;

                paint_voxel.color = glm::vec3(0.0, 0.0, color_value);
                paint_voxel.visible = true;

                voxel_editor.set((glm::ivec3)new_pos.pos - glm::ivec3(0, 1, 0), paint_voxel);
            }
    });
}


int main() {
    Engine3D engine = Engine3D();
    Window window = Window(&engine, 1280, 720, "3D visualization");
    engine.set_window(&window);
    ui::init(window.window);
    
    window.disable_cursor();

    ShaderManager shader_manager = ShaderManager(executable_dir_str());

    Camera camera;
    window.set_camera(&camera);

    FPSCameraController camera_controller = FPSCameraController(&camera);
    camera_controller.speed = 50;

    // VoxelGrid voxel_grid = VoxelGrid({16, 16, 16}, 1.0f, {24, 6, 24});
    // float chunk_render_size = voxel_grid.chunk_size.x * voxel_grid.voxel_size;

    // VoxelRasterizatorGPU voxel_rastorizator(&voxel_grid, shader_manager);

    // VtkMeshLoader vtk_mesh_loader = VtkMeshLoader(vertex_layout);

    // MeshData model_mesh_data = vtk_mesh_loader.load_mesh((executable_dir() / "models" / "test_mesh.vtk").string());
    // Mesh model = Mesh(model_mesh_data.vertices, model_mesh_data.indices, &vertex_layout); 
    // model.position = {chunk_render_size * 0, chunk_render_size * 5, chunk_render_size * 0};
    // model.scale = glm::vec3(5.0f);

    VoxelGridGPU voxel_grid_gpu = VoxelGridGPU(
        {16, 16, 16}, // chunk_size
        {1.0f, 1.0f, 1.0f}, // voxel_size
        100'000, // count_active_chunks
        30'000'000, // max_quads
        4, // chunk_hash_table_size_factor
        512, // max_count_probing
        64, // count_evict_buckets
        4'000, // min_free_chunks
        10'000, // max_evict_chunks
        32, // bucket_step
        shader_manager
    );

    // std::vector<glm::ivec3> positions;
    // std::vector<VoxelGridGPU::VoxelDataGPU> voxels;
    // create_occlusion_test_box({0, 0, 0}, positions, voxels);
    // voxel_grid_gpu.apply_writes_to_world_from_cpu(positions, voxels);

    glm::vec3 prev_cam_pos = camera_controller.camera->position;

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

        
        voxel_grid_gpu.stream_chunks_sphere(camera_controller.camera->position, 5, 45345345);
        window.draw(&voxel_grid_gpu, &camera);

        if (glfwGetKey(window->window, GLFW_KEY_R) == GLFW_PRESS) {
            glm::ivec3 voxel_pos = glm::ivec3(glm::floor(camera->position));
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
            voxel_grid_gpu.print_counters(1, 1, 1, 1, 1);
            std::cout << "-----------------------" << std::endl << std::endl;
        }

        if (ImGui::Button("mark_chunk_to_generate()")) {
            voxel_grid_gpu.mark_chunk_to_generate(camera_controller.camera->position, 2);
            std::cout << "mark_chunk_to_generate()" << std::endl;
        }

        if (ImGui::Button("generate_terrain()")) {
            uint32_t load_count = 0;
            voxel_grid_gpu.stream_counters_.read_subdata(0, &load_count, sizeof(uint32_t));
            if (load_count != 0) {
                voxel_grid_gpu.generate_terrain(45345345, load_count);
                voxel_grid_gpu.mark_all_used_chunks_as_dirty();
                std::cout << "generate_terrain()" << std::endl;
            } else {
                std::cout << "load_count == 0" << std::endl;
            }
            
        }


                flush(cur_dir);
            }
        }
        ImGui::Begin("Debug");

        glm::vec3 p = camera->position; // or any vec3

        ImGui::TextColored(ImVec4(1,0.5,0.5,1), "x: %.3f", p.x);
        ImGui::TextColored(ImVec4(0.5,1,0.5,1), "y: %.3f", p.y);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "z: %.3f", p.z);

        ImGui::End();

        ui::end_frame();

        window.swap_buffers();
        engine.poll_events();

        prev_cam_pos = camera_controller.camera->position;
    }
    
    ui::shutdown();
}
