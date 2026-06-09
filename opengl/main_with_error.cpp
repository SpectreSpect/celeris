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
#include "a_star/a_star.h"
#include "line.h"
#include "a_star/nonholonomic_a_star.h"
#include "a_star/reeds_shepp.h"

#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include "point.h"
#include "point_cloud/point_cloud_frame.h"
#include "point_cloud/point_cloud_video.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <cmath>
#include <cstdint>
#include "math_utils.h"
#include "light_source.h"
#include "circle_cloud.h"
#include "texture.h"
#include "cubemap.h"
#include "skybox.h"
#include "framebuffer.h"
#include "sphere.h"
#include "cube.h"
#include "quad.h"
#include "texture_manager.h"
#include "pbr_skybox.h"
#include <algorithm>
#include <random>
#include "gicp.h"

struct alignas(16) HashPoint {
    glm::vec4 position;
    glm::vec4 normal;
};

struct alignas(16) PointNormal {
    glm::vec4 normal;
};

struct alignas(16) HashTableSlot {
    glm::ivec2     hash_key;
    std::uint32_t  _pad0[2];     // forces hash_value to offset 16

    HashPoint      hash_value;   // offset 16, size 32
    std::uint32_t  hash_state;   // offset 48
    std::uint32_t  _pad1[3];     // tail pad so sizeof == 64
};

struct alignas(16) Mat3Std430 {
    glm::vec4 col0; // use xyz, w is padding
    glm::vec4 col1; // use xyz, w is padding
    glm::vec4 col2; // use xyz, w is padding
};

struct HAndG {
    double h[6][6];
    double g[6];
    uint status; // lock flag
};


// struct OutputHG {
//     double h[6][6];
//     double g[6];
// };

struct alignas(16) Location {
    glm::vec4 position;
    glm::vec4 rotation;
};

struct alignas(8) TracePoint {
    uint32_t counter;
    uint32_t site_id;
    double value;
    uint32_t pad0;
};


class VoxelMap {
public:
    SSBO map_hash_table_ssbo;
    SSBO map_points_ssbo;
    SSBO map_normals_ssbo;
    SSBO num_map_points_ssbo;

    SSBO h_and_g_ssbo;
    SSBO source_location_ssbo;

    SSBO Mat3Std430_ssbo;

    SSBO transform_normal_world_ssbo;
    SSBO out_val_ssbo;

    SSBO target_ids_ssbo;
    SSBO output_hg;
    SSBO trace_ssbo;
    SSBO num_trace_points_ssbo;
    

    uint32_t num_map_points = 0;
    ComputeProgram* add_point_cloud_program;
    ComputeProgram* align_point_cloud_program;

    int max_map_points_count = 0;

    VoxelMap(ShaderManager& shader_manager, 
             ComputeProgram& add_point_cloud_program, ComputeProgram& align_point_cloud_program, 
             float voxel_size, int hash_table_size, int max_map_points_count) {
        this->max_map_points_count = max_map_points_count;

        map_hash_table_ssbo = SSBO::from_fill(sizeof(std::uint32_t) * 4 + sizeof(HashTableSlot) * hash_table_size, GL_DYNAMIC_DRAW, 0u, shader_manager);
        map_points_ssbo = SSBO::from_fill(sizeof(PointInstance) * (max_map_points_count), GL_DYNAMIC_DRAW, 0u, shader_manager);
        map_normals_ssbo = SSBO::from_fill(sizeof(PointNormal) * (max_map_points_count), GL_DYNAMIC_DRAW, 0u, shader_manager);
        target_ids_ssbo = SSBO::from_fill(sizeof(uint32_t) * (max_map_points_count), GL_DYNAMIC_DRAW, 0u, shader_manager);\
        output_hg = SSBO::from_fill(sizeof(OutputHG) * (max_map_points_count), GL_DYNAMIC_DRAW, 0u, shader_manager);
        trace_ssbo = SSBO::from_fill(sizeof(TracePoint) * (5000), GL_DYNAMIC_DRAW, 0u, shader_manager);
        num_trace_points_ssbo = SSBO::from_fill(sizeof(uint32_t), GL_DYNAMIC_DRAW, 0u, shader_manager);
        num_map_points_ssbo = SSBO::from_fill(sizeof(uint32_t), GL_DYNAMIC_DRAW, 0u, shader_manager);
        h_and_g_ssbo = SSBO::from_fill(sizeof(HAndG), GL_DYNAMIC_DRAW, 0u, shader_manager);
        source_location_ssbo = SSBO::from_fill(sizeof(Location), GL_DYNAMIC_DRAW, 0u, shader_manager);
        Mat3Std430_ssbo = SSBO::from_fill(sizeof(Mat3Std430), GL_DYNAMIC_DRAW, 0u, shader_manager);
        transform_normal_world_ssbo = SSBO::from_fill(sizeof(glm::vec4), GL_DYNAMIC_DRAW, 0u, shader_manager);
        out_val_ssbo = SSBO::from_fill(sizeof(glm::dvec4), GL_DYNAMIC_DRAW, 0u, shader_manager);
        
        
        this->add_point_cloud_program = &add_point_cloud_program;
        this->align_point_cloud_program = &align_point_cloud_program;
    }

    void add_point_cloud(Point& source_point_cloud, SSBO& source_normals_ssbo) {
        // std::uint32_t num_points = point_cloud_video.frames[0].point_cloud.points.size();
        SSBO source_points_ssbo = SSBO(*source_point_cloud.instance_vbo);
        std::uint32_t num_source_points = source_point_cloud.instance_count;
        int x_count = math_utils::div_up_u32(num_source_points, 256);
        
        map_hash_table_ssbo.bind_base(0);
        
        map_points_ssbo.bind_base(1);
        map_normals_ssbo.bind_base(2);
        num_map_points_ssbo.bind_base(3);
        
        source_points_ssbo.bind_base(4);
        source_normals_ssbo.bind_base(5);
        
        
        // // add_point_cloud_to_map_program.set_uint("num_points", num_points);
        add_point_cloud_program->set_uint("num_source_points", num_source_points);

        add_point_cloud_program->use();
        add_point_cloud_program->dispatch_compute(x_count, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        num_map_points_ssbo.read_subdata(0u, &num_map_points, sizeof(uint32_t));

        // PointNormal normals[5];
        // map_normals_ssbo.read_subdata(0u, &normals, sizeof(PointNormal) * 5);

        // for (int i = 0; i < 5; i++) {
        //     std::cout << i << ") " << "normal: (" << normals[i].normal.x << ", " << normals[i].normal.y << ", " << normals[i].normal.z << ")" << std::endl;
        // }
    }

    void gicp_step_gpu(PointCloud& source_point_cloud, SSBO& source_normals_ssbo) {
        Location source_location;
        source_location.position = glm::vec4(source_point_cloud.position, 1.0f);
        source_location.rotation = glm::vec4(source_point_cloud.rotation, 1.0f);

        source_location_ssbo.update_subdata(0, &source_location, sizeof(Location));

        SSBO source_points_ssbo = SSBO(*source_point_cloud.point_renderer.instance_vbo);
        std::uint32_t num_source_points = source_point_cloud.point_renderer.instance_count;
        int x_count = math_utils::div_up_u32(num_source_points, 256);
        
        map_hash_table_ssbo.bind_base(0);
        
        map_points_ssbo.bind_base(1);
        map_normals_ssbo.bind_base(2);
        num_map_points_ssbo.bind_base(3);
        
        source_points_ssbo.bind_base(4);
        source_normals_ssbo.bind_base(5);
        h_and_g_ssbo.bind_base(6);
        source_location_ssbo.bind_base(7);
        out_val_ssbo.bind_base(9);
        target_ids_ssbo.bind_base(10);
        output_hg.bind_base(11);
        trace_ssbo.bind_base(12);
        num_trace_points_ssbo.bind_base(13);
        
        
        // // add_point_cloud_to_map_program.set_uint("num_points", num_points);
        align_point_cloud_program->use();
        align_point_cloud_program->set_uint("num_source_points", num_source_points);
        
        align_point_cloud_program->dispatch_compute(x_count, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        num_map_points_ssbo.read_subdata(0u, &num_map_points, sizeof(uint32_t));

        Location location_out = {};
        source_location_ssbo.read_subdata(0u, &location_out, sizeof(Location));

        source_point_cloud.position = location_out.position;
        source_point_cloud.rotation = location_out.rotation;

        std::cout << "position: (" << location_out.position.x << ", " << location_out.position.y << ", " << location_out.position.z << ")" << std::endl;
        std::cout << "rotation: (" << location_out.rotation.x << ", " << location_out.rotation.y << ", " << location_out.rotation.z << ")" << std::endl;


        glm::dvec4 out = {};
        out_val_ssbo.read_subdata(0u, &out, sizeof(glm::dvec4));

        
        std::cout << "bool ok = " << out.x << std::endl;

        HAndG out_h_and_g = {};
        output_hg.read_subdata(0u, &out_h_and_g, sizeof(HAndG));

        std::cout << std::setprecision(std::numeric_limits<double>::max_digits10);

        std::cout << "double delta[6] = {";
        for (int i = 0; i < 6; ++i) {
            std::cout << out_h_and_g.g[i];
            if (i < 5) std::cout << ", ";
        }
        std::cout << "};\n";

        uint32_t num_trace_points = 0;
        num_trace_points_ssbo.read_subdata(0u, &num_trace_points, sizeof(uint32_t));

        TracePoint out_trace_points[num_trace_points];
        trace_ssbo.read_subdata(0u, out_trace_points, sizeof(TracePoint) * num_trace_points);

        std::ofstream out_stream("/home/spectre/Projects/test_open_3d/solve_6x6_gpu_dump.txt");
        out_stream << std::setprecision(std::numeric_limits<double>::max_digits10);

        for (int i = 0; i < num_trace_points; i++) {
            out_stream << out_trace_points[i].counter << "." << out_trace_points[i].site_id << " = " << out_trace_points[i].value << '\n';
        }
        out_stream.close();
        
            

        // for (int i = 0; i < 6; i++) {
        //     for (int j = 0; j < 6; j++) {
        //         std::cout << "h[" << i << "][" << j << "] = " << out_h_and_g.h[i][j] << std::endl;
        //     }
        // }

        // std::cout << std::endl;

        // for (int i = 0; i < 6; i++) {
        //     std::cout << "g[" << i << "] = " << out_h_and_g.g[i] << std::endl;
        // }
    }

    void align_point_cloud(PointCloud& source_point_cloud, SSBO& source_normals_ssbo) {
        source_point_cloud.sync_gpu();
        SSBO source_points_ssbo = SSBO(*source_point_cloud.point_renderer.instance_vbo);
        std::uint32_t num_source_points = source_point_cloud.point_renderer.instance_count;
        int x_count = math_utils::div_up_u32(num_source_points, 256);
        
        map_hash_table_ssbo.bind_base(0);
        
        map_points_ssbo.bind_base(1);
        map_normals_ssbo.bind_base(2);
        num_map_points_ssbo.bind_base(3);
        
        source_points_ssbo.bind_base(4);
        source_normals_ssbo.bind_base(5);
        h_and_g_ssbo.bind_base(6);
        
        // // add_point_cloud_to_map_program.set_uint("num_points", num_points);
        align_point_cloud_program->set_uint("num_source_points", num_source_points);

        align_point_cloud_program->use();
        align_point_cloud_program->dispatch_compute(x_count, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        num_map_points_ssbo.read_subdata(0u, &num_map_points, sizeof(uint32_t));

        HAndG out_h_and_g = {};
        h_and_g_ssbo.read_subdata(0u, &out_h_and_g, sizeof(HAndG));

        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < 6; j++) {
                std::cout << "h[" << i << "][" << j << "] = " << out_h_and_g.h[i][j] << std::endl;
            }
        }

        std::cout << std::endl;

        for (int i = 0; i < 6; i++) {
            std::cout << "g[" << i << "] = " << out_h_and_g.g[i] << std::endl;
        }
    }

    glm::mat3 test(PointCloud& source_point_cloud, SSBO& source_normals_ssbo, glm::vec3 euler) {
        source_point_cloud.sync_gpu();
        SSBO source_points_ssbo = SSBO(*source_point_cloud.point_renderer.instance_vbo);
        std::uint32_t num_source_points = source_point_cloud.point_renderer.instance_count;
        int x_count = math_utils::div_up_u32(num_source_points, 256);
        
        map_hash_table_ssbo.bind_base(0);
        
        map_points_ssbo.bind_base(1);
        map_normals_ssbo.bind_base(2);
        num_map_points_ssbo.bind_base(3);
        
        source_points_ssbo.bind_base(4);
        source_normals_ssbo.bind_base(5);
        h_and_g_ssbo.bind_base(6);
        Mat3Std430_ssbo.bind_base(9);
        
        // // add_point_cloud_to_map_program.set_uint("num_points", num_points);
        align_point_cloud_program->set_uint("num_source_points", num_source_points);
        align_point_cloud_program->set_vec3("uEuler", euler);

        align_point_cloud_program->use();
        align_point_cloud_program->dispatch_compute(x_count, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        num_map_points_ssbo.read_subdata(0u, &num_map_points, sizeof(uint32_t));

        Mat3Std430 out_Mat3Std430 = {};
        Mat3Std430_ssbo.read_subdata(0u, &out_Mat3Std430, sizeof(Mat3Std430));

        glm::mat3 result;
        
        for (int i = 0; i < 3; i++) {
            result[0][i] = out_Mat3Std430.col0[i];
        }
        // std::cout << std::endl;
        for (int i = 0; i < 3; i++) {
            result[1][i] = out_Mat3Std430.col1[i];
        }
        // std::cout << std::endl;
        for (int i = 0; i < 3; i++) {
            result[2][i] = out_Mat3Std430.col2[i];
        }
        // std::cout << std::endl;

        return result;
    }

    glm::vec3 transform_normal_world_test(PointCloud& source_point_cloud, SSBO& source_normals_ssbo, const PointCloud& cloud, const glm::vec3& local_n) {
        source_point_cloud.sync_gpu();
        SSBO source_points_ssbo = SSBO(*source_point_cloud.point_renderer.instance_vbo);
        std::uint32_t num_source_points = source_point_cloud.point_renderer.instance_count;
        int x_count = math_utils::div_up_u32(num_source_points, 256);
        
        map_hash_table_ssbo.bind_base(0);
        
        map_points_ssbo.bind_base(1);
        map_normals_ssbo.bind_base(2);
        num_map_points_ssbo.bind_base(3);
        
        source_points_ssbo.bind_base(4);
        source_normals_ssbo.bind_base(5);
        h_and_g_ssbo.bind_base(6);
        transform_normal_world_ssbo.bind_base(9);
        
        // // add_point_cloud_to_map_program.set_uint("num_points", num_points);
        align_point_cloud_program->set_uint("num_source_points", num_source_points);
        align_point_cloud_program->set_vec3("uCloudRotation", cloud.rotation);
        align_point_cloud_program->set_vec3("uCloudScale", cloud.scale);
        align_point_cloud_program->set_vec3("uLocalN", local_n);


        // align_point_cloud_program->set_vec3("uEuler", euler);

        align_point_cloud_program->use();
        align_point_cloud_program->dispatch_compute(x_count, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        num_map_points_ssbo.read_subdata(0u, &num_map_points, sizeof(uint32_t));

        glm::vec4 out = {};
        transform_normal_world_ssbo.read_subdata(0u, &out, sizeof(glm::vec4));

        return out;
    }

    glm::vec3 transform_point_world_test(PointCloud& source_point_cloud, SSBO& source_normals_ssbo, const PointCloud& cloud, const glm::vec3& local_p) {
        source_point_cloud.sync_gpu();
        SSBO source_points_ssbo = SSBO(*source_point_cloud.point_renderer.instance_vbo);
        std::uint32_t num_source_points = source_point_cloud.point_renderer.instance_count;
        int x_count = math_utils::div_up_u32(num_source_points, 256);
        
        map_hash_table_ssbo.bind_base(0);
        
        map_points_ssbo.bind_base(1);
        map_normals_ssbo.bind_base(2);
        num_map_points_ssbo.bind_base(3);
        
        source_points_ssbo.bind_base(4);
        source_normals_ssbo.bind_base(5);
        h_and_g_ssbo.bind_base(6);
        transform_normal_world_ssbo.bind_base(9);
        
        // // add_point_cloud_to_map_program.set_uint("num_points", num_points);
        align_point_cloud_program->set_uint("num_source_points", num_source_points);

        align_point_cloud_program->set_vec3("uCloudRotation", cloud.rotation);
        align_point_cloud_program->set_vec3("uCloudScale", cloud.scale);
        align_point_cloud_program->set_vec3("uCloudPosition", cloud.position);
        align_point_cloud_program->set_vec3("uLocalP", local_p);


        // align_point_cloud_program->set_vec3("uEuler", euler);

        align_point_cloud_program->use();
        align_point_cloud_program->dispatch_compute(x_count, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        num_map_points_ssbo.read_subdata(0u, &num_map_points, sizeof(uint32_t));

        glm::vec4 out = {};
        transform_normal_world_ssbo.read_subdata(0u, &out, sizeof(glm::vec4));

        return out;
    }


    glm::mat3 covariance_from_normal_test(PointCloud& source_point_cloud, SSBO& source_normals_ssbo, glm::vec3 raw_n, float eps) {
        source_point_cloud.sync_gpu();
        SSBO source_points_ssbo = SSBO(*source_point_cloud.point_renderer.instance_vbo);
        std::uint32_t num_source_points = source_point_cloud.point_renderer.instance_count;
        int x_count = math_utils::div_up_u32(num_source_points, 256);
        
        map_hash_table_ssbo.bind_base(0);
        
        map_points_ssbo.bind_base(1);
        map_normals_ssbo.bind_base(2);
        num_map_points_ssbo.bind_base(3);
        
        source_points_ssbo.bind_base(4);
        source_normals_ssbo.bind_base(5);
        h_and_g_ssbo.bind_base(6);
        Mat3Std430_ssbo.bind_base(9);
        
        // // add_point_cloud_to_map_program.set_uint("num_points", num_points);
        align_point_cloud_program->set_uint("num_source_points", num_source_points);
        align_point_cloud_program->set_vec3("uRawN", raw_n);
        align_point_cloud_program->set_float("uEps", eps);

        align_point_cloud_program->use();
        align_point_cloud_program->dispatch_compute(x_count, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        num_map_points_ssbo.read_subdata(0u, &num_map_points, sizeof(uint32_t));

        Mat3Std430 out_Mat3Std430 = {};
        Mat3Std430_ssbo.read_subdata(0u, &out_Mat3Std430, sizeof(Mat3Std430));

        glm::mat3 result;
        
        for (int i = 0; i < 3; i++) {
            result[0][i] = out_Mat3Std430.col0[i];
        }
        // std::cout << std::endl;
        for (int i = 0; i < 3; i++) {
            result[1][i] = out_Mat3Std430.col1[i];
        }
        // std::cout << std::endl;
        for (int i = 0; i < 3; i++) {
            result[2][i] = out_Mat3Std430.col2[i];
        }
        // std::cout << std::endl;

        return result;
    }

    // vec3 mat3_to_euler_xyz(mat3 R)

    glm::vec3 mat3_to_euler_xyz_test(PointCloud& source_point_cloud, SSBO& source_normals_ssbo, const glm::mat3& R) {
        source_point_cloud.sync_gpu();
        SSBO source_points_ssbo = SSBO(*source_point_cloud.point_renderer.instance_vbo);
        std::uint32_t num_source_points = source_point_cloud.point_renderer.instance_count;
        int x_count = math_utils::div_up_u32(num_source_points, 256);
        
        map_hash_table_ssbo.bind_base(0);
        
        map_points_ssbo.bind_base(1);
        map_normals_ssbo.bind_base(2);
        num_map_points_ssbo.bind_base(3);
        
        source_points_ssbo.bind_base(4);
        source_normals_ssbo.bind_base(5);
        h_and_g_ssbo.bind_base(6);
        transform_normal_world_ssbo.bind_base(9);
        
        // // add_point_cloud_to_map_program.set_uint("num_points", num_points);
        align_point_cloud_program->set_uint("num_source_points", num_source_points);

        align_point_cloud_program->set_mat3("uR", R);


        // align_point_cloud_program->set_vec3("uEuler", euler);

        align_point_cloud_program->use();
        align_point_cloud_program->dispatch_compute(x_count, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        num_map_points_ssbo.read_subdata(0u, &num_map_points, sizeof(uint32_t));

        glm::vec4 out = {};
        transform_normal_world_ssbo.read_subdata(0u, &out, sizeof(glm::vec4));

        return out;
    }

    Point get_point_cloud() {
        return Point(map_points_ssbo, (int)num_map_points);
    }
};




void get_points(std::vector<PointInstance>& points, std::vector<glm::vec4>& normals, glm::vec4 color) {
    // int size = 10;
    // // float step = 0.2f; // distance between grid points
    // float step = 2.0f; // distance between grid points
    // for (int x = 0; x < size; x++) {
    //     for (int z = 0; z < size; z++) {
    //         PointInstance point;
    //         point.color = color;

    //         // Center the grid around (0, 0)
    //         float px = (x - (size - 1) * 0.5f) * step;
    //         float pz = (z - (size - 1) * 0.5f) * step;

    //         // Surface: y = f(x, z)
    //         float py =
    //             0.15f * sinf(1.7f * px) +
    //             0.10f * cosf(1.3f * pz) +
    //             0.05f * px * pz;

    //         point.pos = glm::vec4(px, py, pz, 1.0f);

    //         // Partial derivatives of f(x, z)
    //         float dfdx =
    //             0.15f * 1.7f * cosf(1.7f * px) +
    //             0.05f * pz;

    //         float dfdz =
    //         -0.10f * 1.3f * sinf(1.3f * pz) +
    //             0.05f * px;

    //         // For surface y = f(x,z), normal is (-df/dx, 1, -df/dz)
    //         glm::vec3 n = glm::normalize(glm::vec3(-dfdx, 1.0f, -dfdz));
    //         glm::vec4 normal = glm::vec4(n, 0.0f);
    //         // glm::vec4 normal = n;

    //         points.push_back(point);
    //         normals.push_back(normal);
    //     }
    // }

    PointInstance point;
    point.pos = glm::vec4(0, 0, 0, 1);
    point.color = color;

    points.push_back(point);
    normals.push_back(glm::vec4(0, 1, 0, 1));
}

void get_normals_ssbo(std::vector<glm::vec4> &normals, SSBO& normals_ssbo) {
    normals_ssbo = SSBO(normals.size() * sizeof(glm::vec4), GL_DYNAMIC_DRAW, normals.data());
}


struct TraceLine {
    unsigned long long counter;
    unsigned long long site_id;
    double value;
};

bool parse_line(const std::string& s, TraceLine& out) {
    return std::sscanf(s.c_str(), "%llu.%llu = %lf",
                       &out.counter, &out.site_id, &out.value) == 3;
}

bool close_enough(double a, double b, double abs_eps = 1e-3, double rel_eps = 1e-6) {
    double diff = std::abs(a - b);
    if (diff <= abs_eps) {
        return true;
    }

    double scale = std::max(std::abs(a), std::abs(b));
    return diff <= rel_eps * scale;
}




float clear_col[4] = {0.1f, 0.1f, 0.1f, 1.0f};

int main() {
    Engine3D engine = Engine3D();
    Window window = Window(&engine, 1280, 720, "3D visualization");
    engine.set_window(&window);
    ui::init(window.window);

    ComputeProgram add_point_cloud_to_map_program = ComputeProgram(&engine.shader_manager->add_point_cloud_to_map_cs);
    ComputeProgram align_point_cloud_program = ComputeProgram(&engine.shader_manager->align_point_cloud_cs);

    Camera camera;
    window.set_camera(&camera);
    FPSCameraController camera_controller = FPSCameraController(&camera);
    camera_controller.speed = 50;

    // PointCloudVideo point_cloud_video = PointCloudVideo();
    // point_cloud_video.load_from_file("/home/spectre/TEMP_lidar_output_mesh/recording/index.csv", 5);

    PointCloud target_point_cloud;
    PointCloud source_point_cloud;

    std::vector<PointInstance> target_points;
    std::vector<glm::vec4> target_normals;
    SSBO target_normals_ssbo;

    std::vector<PointInstance> source_points;
    std::vector<glm::vec4> source_normals;
    SSBO source_normals_ssbo;

    get_points(target_points, target_normals, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    get_points(source_points, source_normals, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

    get_normals_ssbo(target_normals, target_normals_ssbo);
    get_normals_ssbo(source_normals, source_normals_ssbo);

    target_point_cloud.update_points(target_points);
    source_point_cloud.update_points(source_points);

    target_point_cloud.sync_gpu();
    source_point_cloud.sync_gpu();

    // source_point_cloud.position = glm::vec4(0.8f, 0.2f, 1.0f, 1.0f);
    source_point_cloud.position = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    // source_point_cloud.rotation = glm::vec3(0.6f, 0.5f, 0.1f);

    
    VoxelMap voxel_map = VoxelMap(*engine.shader_manager, add_point_cloud_to_map_program, align_point_cloud_program, 1, 100000, 1000);
    
    // point_cloud_video.frames[0].point_cloud.sync_gpu();
    // align_point_cloud
    voxel_map.add_point_cloud(target_point_cloud.point_renderer, target_normals_ssbo);


    // std::vector<glm::mat3> r_tests = {
    //     glm::mat3(1.0f),

    //     GICP::euler_xyz_to_mat3(glm::radians(glm::vec3(10.0f, 0.0f, 0.0f))),
    //     GICP::euler_xyz_to_mat3(glm::radians(glm::vec3(0.0f, 20.0f, 0.0f))),
    //     GICP::euler_xyz_to_mat3(glm::radians(glm::vec3(0.0f, 0.0f, 30.0f))),

    //     GICP::euler_xyz_to_mat3(glm::radians(glm::vec3(15.0f, 25.0f, 35.0f))),
    //     GICP::euler_xyz_to_mat3(glm::radians(glm::vec3(-20.0f, 45.0f, -10.0f))),
    //     GICP::euler_xyz_to_mat3(glm::radians(glm::vec3(60.0f, -30.0f, 120.0f))),

    //     // near gimbal lock
    //     GICP::euler_xyz_to_mat3(glm::radians(glm::vec3(89.0f, 1.0f, -45.0f))),
    //     GICP::euler_xyz_to_mat3(glm::radians(glm::vec3(-89.0f, 2.0f, 30.0f))),
    //     GICP::euler_xyz_to_mat3(glm::radians(glm::vec3(90.0f, 0.0f, 0.0f))),
    //     GICP::euler_xyz_to_mat3(glm::radians(glm::vec3(-90.0f, 0.0f, 0.0f)))
    // };

    // const float eps = 1e-4f;

    // auto is_close_mat3 = [&](const glm::mat3& a, const glm::mat3& b, float eps) {
    //     for (int c = 0; c < 3; c++) {
    //         for (int r = 0; r < 3; r++) {
    //             if (std::abs(a[c][r] - b[c][r]) > eps)
    //                 return false;
    //         }
    //     }
    //     return true;
    // };

    // for (int i = 0; i < r_tests.size(); i++) {
    //     glm::vec3 result_cpu = GICP::mat3_to_euler_xyz(r_tests[i]);
    //     glm::vec3 result_gpu = voxel_map.mat3_to_euler_xyz_test(
    //         source_point_cloud,
    //         source_normals_ssbo,
    //         r_tests[i]
    //     );

    //     glm::mat3 recon_cpu = GICP::euler_xyz_to_mat3(result_cpu);
    //     glm::mat3 recon_gpu = GICP::euler_xyz_to_mat3(result_gpu);

    //     bool cpu_ok = is_close_mat3(r_tests[i], recon_cpu, eps);
    //     bool gpu_ok = is_close_mat3(r_tests[i], recon_gpu, eps);
    //     bool cpu_gpu_same_rotation = is_close_mat3(recon_cpu, recon_gpu, eps);

    //     std::cout << "Test " << i << ": "
    //             << ((cpu_ok && gpu_ok && cpu_gpu_same_rotation) ? "OK" : "MISMATCH")
    //             << "\n";

    //     std::cout << "  CPU euler = ("
    //             << result_cpu.x << ", "
    //             << result_cpu.y << ", "
    //             << result_cpu.z << ")\n";

    //     std::cout << "  GPU euler = ("
    //             << result_gpu.x << ", "
    //             << result_gpu.y << ", "
    //             << result_gpu.z << ")\n";

    //     if (!(cpu_ok && gpu_ok && cpu_gpu_same_rotation)) {
    //         std::cout << "  cpu_ok = " << cpu_ok
    //                 << ", gpu_ok = " << gpu_ok
    //                 << ", cpu_gpu_same_rotation = " << cpu_gpu_same_rotation
    //                 << "\n";

    //         std::cout << "  Original R:\n";
    //         for (int r = 0; r < 3; r++) {
    //             std::cout << r_tests[i][0][r] << " "
    //                     << r_tests[i][1][r] << " "
    //                     << r_tests[i][2][r] << "\n";
    //         }

    //         std::cout << "  Reconstructed CPU R:\n";
    //         for (int r = 0; r < 3; r++) {
    //             std::cout << recon_cpu[0][r] << " "
    //                     << recon_cpu[1][r] << " "
    //                     << recon_cpu[2][r] << "\n";
    //         }

    //         std::cout << "  Reconstructed GPU R:\n";
    //         for (int r = 0; r < 3; r++) {
    //             std::cout << recon_gpu[0][r] << " "
    //                     << recon_gpu[1][r] << " "
    //                     << recon_gpu[2][r] << "\n";
    //         }
    //     }
    // }

    std::vector<PointInstance> test_map_points;
    std::vector<glm::vec4> test_map_normals;

    std::vector<PointInstance> test_source_points;
    std::vector<glm::vec4> test_source_normals;

    

    // map_points_ssbo = SSBO::from_fill(sizeof(PointInstance) * (max_map_points_count), GL_DYNAMIC_DRAW, 0u, shader_manager);

    // voxel_map.

    test_map_points.resize(voxel_map.max_map_points_count);
    voxel_map.map_points_ssbo.read_subdata(0u, test_map_points.data(), sizeof(PointInstance) * voxel_map.max_map_points_count);

    test_map_normals.resize(voxel_map.max_map_points_count);
    voxel_map.map_normals_ssbo.read_subdata(0u, test_map_normals.data(), sizeof(glm::vec4) * voxel_map.max_map_points_count);

    
    std::uint32_t num_source_points = source_point_cloud.point_renderer.instance_count;
    SSBO source_points_ssbo = SSBO(*source_point_cloud.point_renderer.instance_vbo);
    test_source_points.resize(num_source_points);
    source_points_ssbo.read_subdata(0u, test_source_points.data(), sizeof(PointInstance) * num_source_points);

    test_source_normals.resize(num_source_points);
    source_normals_ssbo.read_subdata(0u, test_source_normals.data(), sizeof(glm::vec4) * num_source_points);

    Location test_source_location;
    test_source_location.position = glm::vec4(source_point_cloud.position, 1.0f);
    test_source_location.rotation = glm::vec4(source_point_cloud.rotation, 1.0f);
    

    Point map_points = voxel_map.get_point_cloud();

    
    glm::vec3 original_position = source_point_cloud.position;
    glm::vec3 original_rotation = source_point_cloud.rotation;


    // voxel_map.gicp_step_gpu(source_point_cloud, source_normals_ssbo);
    
    std::vector<uint32_t> target_ids_gpu = {};
    target_ids_gpu.resize(num_source_points);
    voxel_map.target_ids_ssbo.read_subdata(0u, target_ids_gpu.data(), sizeof(uint32_t) * num_source_points);

    std::vector<OutputHG> output_hg_gpu = {};
    output_hg_gpu.resize(num_source_points);
    voxel_map.output_hg.read_subdata(0u, output_hg_gpu.data(), sizeof(OutputHG) * num_source_points);

    std::vector<uint32_t> target_ids_cpu = {};
    target_ids_cpu.resize(num_source_points);

    std::vector<OutputHG> output_hg_cpu = {};
    output_hg_cpu.resize(num_source_points);

    // test_source_location.position = glm::vec4(original_position, 1.0f);
    // test_source_location.rotation = glm::vec4(original_rotation, 1.0f);
    // GICP::step_test2(test_source_points, test_map_points, test_source_normals, test_map_normals, test_source_location.position, test_source_location.rotation, target_ids_cpu, output_hg_cpu);
    // source_point_cloud.position = test_source_location.position;
    // source_point_cloud.rotation = test_source_location.rotation;

    // for (int i = 0; i < num_source_points; i++) {
    //     if (target_ids_gpu[i] != target_ids_cpu[i]) {
    //         std::cout << "MISMATCH" << std::endl;
    //     }
    // }

    // for (int i = 0; i < num_source_points; i++) {
    //     for (int a = 0; a < 6; a++) {
            
    //         if (std::abs(output_hg_cpu[i].g[a] - output_hg_gpu[i].g[a]) > 1e-1) {
    //                 std::cout << "MISMATCH" << std::endl;
    //             }

    //         for (int b = 0; b < 6; b++) {
    //             if (std::abs(output_hg_cpu[i].h[a][b] - output_hg_gpu[i].h[a][b]) > 1e-1) {
    //                 std::cout << "MISMATCH" << std::endl;        
    //             }
    //         }
    //     }
    // }

    // for (int i = 0; i < num_source_points; ++i) {
    //     bool mismatch = false;

    //     for (int a = 0; a < 6; ++a) {
    //         if (std::abs(output_hg_cpu[i].g[a] - output_hg_gpu[i].g[a]) > 1e-1) {
    //             mismatch = true;
    //         }

    //         for (int b = 0; b < 6; ++b) {
    //             if (std::abs(output_hg_cpu[i].h[a][b] - output_hg_gpu[i].h[a][b]) > 1e-1) {
    //                 mismatch = true;
    //             }
    //         }
    //     }

    //     // if (!mismatch) {
    //     //     continue;
    //     // }

    //     std::cout << "\n==================== i = " << i << " ====================\n";

    //     std::cout << "double test_H[6][6] = double[6][6](\n";
    //     for (int a = 0; a < 6; ++a) {
    //         std::cout << "    double[6](";
    //         for (int b = 0; b < 6; ++b) {
    //             std::cout << output_hg_cpu[i].h[a][b];
    //             if (b < 5) std::cout << ", ";
    //         }
    //         std::cout << ")";
    //         if (a < 5) std::cout << ",";
    //         std::cout << "\n";
    //     }
    //     std::cout << ");\n\n";

    //     std::cout << "double test_g[6] = double[6]( ";
    //     for (int a = 0; a < 6; ++a) {
    //         std::cout << output_hg_cpu[i].g[a];
    //         if (a < 5) std::cout << ", ";
    //     }
    //     std::cout << " );\n\n";

    //     std::cout << "double test_delta_out[6];\n";
    //     std::cout << "bool ok = solve_6x6(test_H, test_g, test_delta_out);\n\n";

    //     // Optional: print GPU too for side-by-side comparison
    //     std::cout << "// GPU version\n";
    //     std::cout << "double test_H_gpu[6][6] = double[6][6](\n";
    //     for (int a = 0; a < 6; ++a) {
    //         std::cout << "    double[6](";
    //         for (int b = 0; b < 6; ++b) {
    //             std::cout << output_hg_gpu[i].h[a][b];
    //             if (b < 5) std::cout << ", ";
    //         }
    //         std::cout << ")";
    //         if (a < 5) std::cout << ",";
    //         std::cout << "\n";
    //     }
    //     std::cout << ");\n\n";

    //     std::cout << "double test_g_gpu[6] = double[6]( ";
    //     for (int a = 0; a < 6; ++a) {
    //         std::cout << output_hg_gpu[i].g[a];
    //         if (a < 5) std::cout << ", ";
    //     }
    //     std::cout << " );\n";

    //     break; // print only first mismatch
    // }

    std::ifstream cpu("/home/spectre/Projects/test_open_3d/solve_6x6_cpu_dump.txt");
    std::ifstream gpu("/home/spectre/Projects/test_open_3d/solve_6x6_gpu_dump.txt");

    std::string cpu_line_str, gpu_line_str;
    int line = 1;

    while (true) {
        bool ok_cpu = static_cast<bool>(std::getline(cpu, cpu_line_str));
        bool ok_gpu = static_cast<bool>(std::getline(gpu, gpu_line_str));

        if (!ok_cpu && !ok_gpu) {
            std::cout << "Files match within tolerance\n";
            break;
        }

        if (ok_cpu != ok_gpu) {
            std::cout << "Files have different length at line " << line << "\n";
            break;
        }

        TraceLine cpu_line, gpu_line;
        if (!parse_line(cpu_line_str, cpu_line)) {
            std::cout << "Failed to parse CPU line " << line << ":\n" << cpu_line_str << "\n";
            break;
        }
        if (!parse_line(gpu_line_str, gpu_line)) {
            std::cout << "Failed to parse GPU line " << line << ":\n" << gpu_line_str << "\n";
            break;
        }

        if (cpu_line.counter != gpu_line.counter) {
            std::cout << "Counter mismatch at line " << line << "\n";
            std::cout << "CPU: " << cpu_line_str << "\n";
            std::cout << "GPU: " << gpu_line_str << "\n";
            break;
        }

        if (cpu_line.site_id != gpu_line.site_id) {
            std::cout << "Site ID mismatch at line " << line << "\n";
            std::cout << "CPU: " << cpu_line_str << "\n";
            std::cout << "GPU: " << gpu_line_str << "\n";
            break;
        }

        if (!close_enough(cpu_line.value, gpu_line.value)) {
            std::cout << "Value mismatch at line " << line << "\n";
            std::cout << "CPU: " << cpu_line_str << "\n";
            std::cout << "GPU: " << gpu_line_str << "\n";
            std::cout << "abs diff = " << std::abs(cpu_line.value - gpu_line.value) << "\n";
            break;
        }

        line++;
    }
    

    
    
    
    float rel_thresh = 1.5f;
    float last_frame = 0.0f;
    float timer = 0.0f;
    int cur_frame = 100;
    bool map_initialized = false;
    while(window.is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - last_frame;
        timer += delta_time;
        last_frame = currentFrame;   
        float fps = 1.0f / delta_time;
        
        camera_controller.update(&window, delta_time);

        engine.update_lighting_system(camera, window);
        window.clear_color({clear_col[0], clear_col[1], clear_col[2], clear_col[3]});

        // window.draw(&layer_line, &camera);
        // window.draw(&layer_line_blue, &camera);
        window.draw(&map_points, &camera);


        // window.draw(&target_point_cloud, &camera);
        window.draw(&source_point_cloud, &camera);
        

        ui::begin_frame();
        ui::update_mouse_mode(&window);

        ImGui::Begin("Debug");

        glm::vec3 p = camera.position; // or any vec3

        ImGui::TextColored(ImVec4(1,0.5,0.5,1), "x: %.3f", p.x);
        ImGui::TextColored(ImVec4(0.5,1,0.5,1), "y: %.3f", p.y);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "z: %.3f", p.z);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "fps: %.3f", fps);

        if (ImGui::Button("GICP step")) {
            // GICP::step_test(source_point_cloud, target_point_cloud, source_normals, target_normals)
            
            // std::vector<PointInstance> test_map_points;
            // std::vector<glm::vec4> test_map_normals;

            // std::vector<PointInstance> test_source_points;
            // std::vector<glm::vec4> test_source_normals;



            // test_source_location.position = glm::vec4(source_point_cloud.position, 1.0f);
            // test_source_location.rotation = glm::vec4(source_point_cloud.rotation, 1.0f);
            // GICP::step_test2(test_source_points, test_map_points, test_source_normals, test_map_normals, test_source_location.position, test_source_location.rotation, target_ids_cpu, output_hg_cpu);
            // source_point_cloud.position = test_source_location.position;
            // source_point_cloud.rotation = test_source_location.rotation;


            voxel_map.gicp_step_gpu(source_point_cloud, source_normals_ssbo);
            // voxel_map.test(source_point_cloud, source_normals_ssbo, glm::vec3(1, 1, 1));
        }

        ImGui::End();

        ui::end_frame();
        

        window.swap_buffers();
        engine.poll_events();
    }
    
    ui::shutdown();
}
