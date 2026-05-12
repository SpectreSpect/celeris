#include "gicp_pass.h"
#include <cmath>
#include <iostream>

// glm::mat3 GICPPass::euler_xyz_to_mat3(const glm::vec3& euler) {
//     float cx = std::cos(euler.x);
//     float sx = std::sin(euler.x);

//     float cy = std::cos(euler.y);
//     float sy = std::sin(euler.y);

//     float cz = std::cos(euler.z);
//     float sz = std::sin(euler.z);

//     glm::mat3 rx(1.0f);
//     rx[0] = glm::vec3(1.0f, 0.0f, 0.0f);
//     rx[1] = glm::vec3(0.0f,  cx,  -sx);
//     rx[2] = glm::vec3(0.0f,  sx,   cx);

//     glm::mat3 ry(1.0f);
//     ry[0] = glm::vec3( cy, 0.0f,  sy);
//     ry[1] = glm::vec3(0.0f, 1.0f, 0.0f);
//     ry[2] = glm::vec3(-sy, 0.0f,  cy);

//     glm::mat3 rz(1.0f);
//     rz[0] = glm::vec3( cz, -sz, 0.0f);
//     rz[1] = glm::vec3( sz,  cz, 0.0f);
//     rz[2] = glm::vec3(0.0f, 0.0f, 1.0f);

//     return rz * ry * rx;
// }

glm::mat3 GICPPass::euler_xyz_to_mat3(const glm::vec3& euler) {
    return glm::mat3(glm::eulerAngleXYZ(euler.x, euler.y, euler.z));
}



glm::mat3 GICPPass::skew_matrix(const glm::vec3& v) {
    glm::mat3 K(0.0f);
    K[0] = glm::vec3( 0.0f,  v.z, -v.y);
    K[1] = glm::vec3(-v.z,  0.0f,  v.x);
    K[2] = glm::vec3( v.y, -v.x,  0.0f);
    return K;
}

// bool GICPPass::solve_6x6(const double H_in[6][6], const double g_in[6], double delta_out[6]) {
//     const double EPS = 1.0e-12;
//     double a[6][7];

//     for (int r = 0; r < 6; ++r) {
//         for (int c = 0; c < 6; ++c) {
//             a[r][c] = H_in[r][c];
//         }
//         a[r][6] = g_in[r];
//     }

//     for (int col = 0; col < 6; ++col) {
//         int pivot_row = col;
//         double max_abs = std::abs(a[col][col]);

//         for (int r = col + 1; r < 6; ++r) {
//             double v = std::abs(a[r][col]);
//             if (v > max_abs) {
//                 max_abs = v;
//                 pivot_row = r;
//             }
//         }

//         if (max_abs < EPS) {
//             return false;
//         }

//         if (pivot_row != col) {
//             for (int c = col; c < 7; ++c) {
//                 std::swap(a[col][c], a[pivot_row][c]);
//             }
//         }

//         for (int r = col + 1; r < 6; ++r) {
//             double factor = a[r][col] / a[col][col];
//             for (int c = col; c < 7; ++c) {
//                 a[r][c] -= factor * a[col][c];
//             }
//         }
//     }

//     for (int i = 0; i < 6; ++i) {
//         delta_out[i] = 0.0;
//     }

//     for (int r = 5; r >= 0; --r) {
//         double sum = a[r][6];

//         for (int c = r + 1; c < 6; ++c) {
//             sum -= a[r][c] * delta_out[c];
//         }

//         if (std::abs(a[r][r]) < EPS) {
//             return false;
//         }

//         delta_out[r] = sum / a[r][r];
//     }

//     return true;
// }


bool GICPPass::solve_6x6(const double H_in[6][6], const double g_in[6], double delta_out[6]) {
    // int counter = 0;
    // std::ofstream out("/home/spectre/Projects/test_open_3d/solve_6x6_cpu_dump.txt");
    // out << std::setprecision(std::numeric_limits<double>::max_digits10);

    // auto dump = [&](int ordinal, double value) {
    //     out << counter << "." << ordinal << " = " << value << '\n';
    //     counter++;
    // };

    double a[6][7];

    for (int r = 0; r < 6; r++) {
        // dump(1, static_cast<double>(r));

        for (int c = 0; c < 6; c++) {
            // dump(2, static_cast<double>(c));

            a[r][c] = H_in[r][c];
            // dump(3, a[r][c]);
        }

        a[r][6] = g_in[r];
        // dump(4, a[r][6]);
    }

    // Forward elimination with partial pivoting
    for (int col = 0; col < 6; col++) {
        // dump(5, static_cast<double>(col));

        // Find pivot row
        int pivot_row = col;
        // dump(6, static_cast<double>(pivot_row));

        double max_abs = std::abs(a[col][col]);
        // dump(7, max_abs);

        for (int r = col + 1; r < 6; r++) {
            // dump(8, static_cast<double>(r));

            double v = std::abs(a[r][col]);
            // dump(9, v);

            if (v > max_abs) {
                // dump(10, 1.0); // entered if

                max_abs = v;
                // dump(11, max_abs);

                pivot_row = r;
                // dump(12, static_cast<double>(pivot_row));
            }
        }

        // Singular / degenerate check
        if (max_abs < 1e-12) {
            // dump(13, 1.0); // entered if
            return false;
        }

        // Swap rows if needed
        if (pivot_row != col) {
            // dump(14, 1.0); // entered if

            for (int c = col; c < 7; c++) {
                // dump(15, static_cast<double>(c));

                std::swap(a[col][c], a[pivot_row][c]);

                // dump(16, a[col][c]);
                // dump(17, a[pivot_row][c]);
            }
        }

        // Eliminate rows below
        for (int r = col + 1; r < 6; r++) {
            // dump(18, static_cast<double>(r));

            double factor = a[r][col] / a[col][col];
            // dump(19, factor);

            for (int c = col; c < 7; c++) {
                // dump(20, static_cast<double>(c));

                a[r][c] -= factor * a[col][c];
                // dump(21, a[r][c]);
            }
        }
    }

    // Back substitution
    for (int r = 5; r >= 0; r--) {
        // dump(22, static_cast<double>(r));

        double sum = a[r][6];
        // dump(23, sum);

        for (int c = r + 1; c < 6; c++) {
            // dump(24, static_cast<double>(c));

            sum -= a[r][c] * delta_out[c];
            // dump(25, sum);
        }

        if (std::abs(a[r][r]) < 1e-12) {
            // dump(26, 1.0); // entered if
            return false;
        }

        delta_out[r] = sum / a[r][r];
        // dump(27, delta_out[r]);
    }

    // dump(28, 1.0); // returning true
    return true;
}

// glm::mat3 GICPPass::omega_to_mat3(const glm::vec3& omega) {
//     float theta = glm::length(omega);

//     if (theta < 1e-12f) {
//         return glm::mat3(1.0f);
//     }

//     glm::vec3 axis = omega / theta;
//     glm::mat3 K = skew_matrix(axis);

//     return glm::mat3(1.0f) + std::sin(theta) * K + (1.0f - std::cos(theta)) * (K * K);
// }

glm::mat3 GICPPass::omega_to_mat3(const glm::vec3& omega) {
    float theta = glm::length(omega);

    if (theta < 1e-12f) {
        return glm::mat3(1.0f);
    }

    glm::vec3 axis = omega / theta;
    glm::mat4 R4 = glm::rotate(glm::mat4(1.0f), theta, axis);
    return glm::mat3(R4);
}

// glm::vec3 GICPPass::mat3_to_euler_xyz(const glm::mat3& R) {
//     const float EPS = 1e-6f;
//     const float HALF_PI = 1.57079632679f;

//     float x, y, z;

//     float sy = glm::clamp(-R[0][2], -1.0f, 1.0f);

//     if (sy >= 1.0f - EPS) {
//         y = HALF_PI;
//         z = 0.0f;
//         x = std::atan2(R[1][0], R[1][1]);
//     }
//     else if (sy <= -1.0f + EPS) {
//         y = -HALF_PI;
//         z = 0.0f;
//         x = std::atan2(-R[1][0], R[1][1]);
//     }
//     else {
//         y = std::asin(sy);
//         x = std::atan2(R[1][2], R[2][2]);
//         z = std::atan2(R[0][1], R[0][0]);
//     }

//     return glm::vec3(x, y, z);
// }

glm::vec3 GICPPass::mat3_to_euler_xyz(const glm::mat3& R) {
    float x, y, z;
    glm::extractEulerAngleXYZ(glm::mat4(R), x, y, z);
    return glm::vec3(x, y, z);
}

void GICPPass::create(VulkanEngine& engine) {
    this->engine = &engine;
    compute_queue_family_id = vulkan_utils::find_compute_queue_family(engine.physicalDevice);
    vkGetDeviceQueue(engine.device, compute_queue_family_id, 0, &compute_queue);

    command_pool.create(engine.device, engine.physicalDevice, compute_queue_family_id, compute_queue);
    command_buffer.create(command_pool);

    shader_module.create(engine.device, "shaders/icp/gicp_step.comp.spv");
    uniform_buffer.create(engine, sizeof(GICPPassUniform));
    output_buffer.create(engine, sizeof(OutputBuffer));

    DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();

    builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_COMPUTE_BIT);

    builder.add_storage_buffer(1, VK_SHADER_STAGE_COMPUTE_BIT); // source point buffer
    builder.add_storage_buffer(2, VK_SHADER_STAGE_COMPUTE_BIT); // source normal buffer
    builder.add_storage_buffer(3, VK_SHADER_STAGE_COMPUTE_BIT); // map point count buffer
    builder.add_storage_buffer(4, VK_SHADER_STAGE_COMPUTE_BIT); // map point buffer
    builder.add_storage_buffer(5, VK_SHADER_STAGE_COMPUTE_BIT); // map normal buffer
    builder.add_storage_buffer(6, VK_SHADER_STAGE_COMPUTE_BIT); // output storage buffer
    builder.add_storage_buffer(7, VK_SHADER_STAGE_COMPUTE_BIT); // voxel hash table buffer
    builder.add_storage_buffer(8, VK_SHADER_STAGE_COMPUTE_BIT); // partial output buffer
    builder.add_storage_buffer(9, VK_SHADER_STAGE_COMPUTE_BIT); // rejection buffer

    descriptor_set_bundle = builder.create(engine.device);
    pipeline.create(engine.device, descriptor_set_bundle, shader_module);

    fence = Fence(engine.device);

    // partial_src.create(engine, sizeof(GICPReductor::GICPPartial) * max_partial_count, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    // partial_dst.create(engine, sizeof(GICPReductor::GICPPartial) * max_partial_count, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    partial_src.create(engine, sizeof(GICPReductor::GICPPartial) * max_partial_count);
    partial_dst.create(engine, sizeof(GICPReductor::GICPPartial) * max_partial_count);
    rejection_buffer.create(engine, sizeof(uint32_t) * max_partial_count);
    // debug_buffer.create(engine, sizeof(uint32_t) * max_partial_count);
    reductor = GICPReductor(engine);
}

double GICPPass::step(VoxelPointMap& voxel_point_map, PointCloud& source_point_cloud, VideoBuffer& source_normal_buffer) {
    if (!this->engine) {
        throw std::runtime_error("engine was null");
    }

    GICPPassUniform uniform_data{};
    uniform_data.position = glm::vec4(source_point_cloud.position, 1.0f);
    uniform_data.rotation = glm::vec4(source_point_cloud.rotation, 1.0f);
    uniform_data.num_source_points = source_point_cloud.num_instances;
    uniform_data.num_target_points = voxel_point_map.map_point_count;
    uniform_data.num_hash_table_slots = voxel_point_map.num_hash_table_slots;

    uniform_buffer.update_data(&uniform_data, sizeof(GICPPassUniform));

    descriptor_set_bundle.bind_storage_buffer(1, source_point_cloud.get_instance_buffer());
    descriptor_set_bundle.bind_storage_buffer(2, source_normal_buffer);
    descriptor_set_bundle.bind_storage_buffer(3, voxel_point_map.map_point_count_buffer);
    descriptor_set_bundle.bind_storage_buffer(4, voxel_point_map.map_point_buffer);
    descriptor_set_bundle.bind_storage_buffer(5, voxel_point_map.map_normal_buffer);
    descriptor_set_bundle.bind_storage_buffer(6, output_buffer);
    descriptor_set_bundle.bind_storage_buffer(7, voxel_point_map.map_hash_table_buffer);
    descriptor_set_bundle.bind_storage_buffer(8, partial_src);
    descriptor_set_bundle.bind_storage_buffer(9, rejection_buffer);

    uint32_t x_groups = vulkan_utils::div_up_u32(source_point_cloud.num_instances, 32);

    command_buffer.begin();
    command_buffer.bind_pipeline(pipeline);
    command_buffer.dispatch(x_groups, 1, 1);
    command_buffer.end();

    command_buffer.submit_and_wait(compute_queue, fence);


    // std::vector<GICPReductor::GICPPartial> partials_out;
    // partials_out.resize(source_point_cloud.num_instances);
    // partial_src.read_subdata(0, partials_out.data(), source_point_cloud.num_instances * sizeof(GICPReductor::GICPPartial));

    // uint32_t test_valid_count = 0u;
    // for (int i = 0; i < partials_out.size(); i++) {
    //     test_valid_count += partials_out[i].valid_count;
    // }

    // std::cout << test_valid_count << std::endl;



    // std::vector<uint32_t> rejection_buffer_out;
    // rejection_buffer_out.resize(source_point_cloud.num_instances);
    // rejection_buffer.read_subdata(0, rejection_buffer_out.data(), source_point_cloud.num_instances * sizeof(uint32_t));


    // std::unordered_map<uint32_t, uint32_t> rejection_counts;

    // for (int i = 0; i < rejection_buffer_out.size(); i++) {
    //     rejection_counts[rejection_buffer_out[i]]++;
    // }

    // for (const auto& [type, count] : rejection_counts) {
    //     std::cout << "Rejection type " << type
    //             << ": " << count << '\n';
    // }


    // OutputBuffer debug_data{};
    // output_buffer.read_subdata(0, &debug_data, sizeof(OutputBuffer));

    // std::cout <<  "(" << debug_data.position.x << ", " << debug_data.position.y << ", " << debug_data.position.z << ", " << debug_data.position.w << ")" << std::endl;
    // std::cout <<  "(" << debug_data.rotation.x << ", " << debug_data.rotation.y << ", " << debug_data.rotation.z << ", " << debug_data.rotation.w << ")" << std::endl;
    
    uint32_t partial_count = vulkan_utils::div_up_u32(source_point_cloud.num_instances, 32);
    GICPReductor::GICPPartial result = reductor.reduce(partial_src, partial_dst, partial_count);

    const float max_rot = glm::radians(5.0f);
    const float max_trans = 5.0f;


    if (result.valid_count < 6) {
        std::cout << "valid_count was less than 6" << std::endl;
        return 99999;
    }

    double rmse = std::sqrt(result.total_weighted_sq_error / double(result.valid_count));

    const double lambda = 1e-6;
    for (int i = 0; i < 6; ++i) {
        result.H[i][i] += lambda;
    }

    double delta[6];
    for (int i = 0; i < 6; ++i) {
        delta[i] = 0.0;
    }

    if (!solve_6x6(result.H, result.g, delta)) {
        std::cout << "solve_6x6 failed" << std::endl;
        return 9999;
    }

    glm::vec3 omega = glm::vec3((float)delta[0], (float)delta[1], (float)delta[2]);

    glm::vec3 v = glm::vec3((float)delta[3], (float)delta[4], (float)delta[5]);

    float omega_len = glm::length(omega);
    if (omega_len > max_rot) {
        omega *= max_rot / omega_len;
    }

    float v_len = glm::length(v);
    if (v_len > max_trans) {
        v *= max_trans / v_len;
    }

    glm::mat3 dR = omega_to_mat3(omega);

    glm::mat3 R_src = euler_xyz_to_mat3(source_point_cloud.rotation);
    glm::mat3 R_src_new = dR * R_src;

    glm::vec3 t_src_new = source_point_cloud.position + v;

    source_point_cloud.position = t_src_new;
    source_point_cloud.rotation = mat3_to_euler_xyz(R_src_new);

    // std::cout << "valid_count = " << result.valid_count
    //           << ", weighted_rmse = " << rmse
    //           << ", |omega| = " << glm::length(omega)
    //           << ", |v| = " << glm::length(v)
    //           << "\n";

    // std::cout << rmse << std::endl;
    return rmse;
}


double GICPPass::fit(VoxelPointMap& voxel_point_map, PointCloud& source_point_cloud, VideoBuffer& source_normal_buffer, uint32_t max_steps) {
    double rmse = 0;
    for (int i = 0; i < max_steps; i++) {
        rmse = step(voxel_point_map, source_point_cloud, source_normal_buffer);
        if (rmse < 0.5) {
            // std::cout << "BREAK" << std::endl;
            break;
        }
            
    }
    // std::cout << rmse << std::endl;

    return rmse;
}