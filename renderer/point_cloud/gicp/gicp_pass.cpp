#include "gicp_pass.h"

#include "../../../vulkan_self/vulkan_engine.h"
#include "../../compute_pass_manager.h"
#include "../point_cloud.h"
#include "voxel_point_map.h"
#include "../../../math_utils.h"

#include <glm/gtx/euler_angles.hpp>


GICPPass::GICPPass(VulkanEngine& engine, ComputePassManager& compute_pass_manager)
    :   engine(engine),
        gicp_step_pass(compute_pass_manager.descriptor_pool(), compute_pass_manager.gicp_cp),
        reductor(engine, compute_pass_manager),
        uniform_buffer(VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(GICPPassUniform))),
        output_buffer(VulkanBuffer::create_host_visible_storage_buffer(engine, sizeof(OutputBuffer))),
        partial_src(VulkanBuffer::create_host_visible_storage_buffer(engine, sizeof(GICPReductor::GICPPartial) * max_partial_count)),
        partial_dst(VulkanBuffer::create_host_visible_storage_buffer(engine, sizeof(GICPReductor::GICPPartial) * max_partial_count)),
        rejection_buffer(VulkanBuffer::create_storage_buffer(engine, sizeof(uint32_t) * max_partial_count)),
        compute_command_buffer(engine.device(), engine.compute_command_pool()),
        compute_fence(engine.device()){
    

    // partial_src.create(engine, sizeof(GICPReductor::GICPPartial) * max_partial_count);
    // partial_dst.create(engine, sizeof(GICPReductor::GICPPartial) * max_partial_count);
    // rejection_buffer.create(engine, sizeof(uint32_t) * max_partial_count);
    // // debug_buffer.create(engine, sizeof(uint32_t) * max_partial_count);
    // reductor = GICPReductor(engine);
}

double GICPPass::step(VoxelPointMap& voxel_point_map, PointCloud& source_point_cloud, VulkanBuffer& source_normal_buffer) {
    LOG_METHOD();

    logger.check(source_point_cloud.instance_buffer_view_valid(), "Source point cloud instance view was invalid");
    logger.check(source_point_cloud.instance_count() > 0, "Source point cloud was empty");
    logger.check(voxel_point_map.m_map_point_count > 0, "The voxel point map didn't have any points");
    logger.check(voxel_point_map.m_num_hash_table_slots > 0, "The voxel point map has table has 0 slots");
    
    glm::quat q = glm::normalize(source_point_cloud.transform.rotation);

    GICPPassUniform uniform_data{};
    uniform_data.position = glm::vec4(source_point_cloud.transform.position, 1.0f);
    // uniform_data.rotation = glm::vec4(source_point_cloud.rotation, 1.0f);
    
    uniform_data.rotation = glm::vec4(q.x, q.y, q.z, q.w);
    uniform_data.num_source_points = source_point_cloud.instance_count();
    uniform_data.num_target_points = voxel_point_map.m_map_point_count; // 1824 2067 2186 2090
    uniform_data.num_hash_table_slots = voxel_point_map.m_num_hash_table_slots;

    uniform_buffer.upload(&uniform_data, sizeof(GICPPassUniform));

    gicp_step_pass.set_uniform_buffer(0, uniform_buffer);
    gicp_step_pass.set_storage_buffer(1, *source_point_cloud.instance_buffer());
    gicp_step_pass.set_storage_buffer(2, source_normal_buffer);
    gicp_step_pass.set_storage_buffer(3, voxel_point_map.map_point_count_buffer);
    gicp_step_pass.set_storage_buffer(4, voxel_point_map.map_point_buffer);
    gicp_step_pass.set_storage_buffer(5, voxel_point_map.map_normal_buffer);
    gicp_step_pass.set_storage_buffer(6, output_buffer);
    gicp_step_pass.set_storage_buffer(7, voxel_point_map.map_hash_table_buffer);
    gicp_step_pass.set_storage_buffer(8, partial_src);
    gicp_step_pass.set_storage_buffer(9, rejection_buffer);

    uint32_t x_groups = math_utils::div_up_u32(source_point_cloud.instance_count(), 32);

    {
        auto compute_scope = compute_command_buffer.begin_scope();
        
        gicp_step_pass.bind(compute_command_buffer);

        compute_command_buffer.dispatch(x_groups, 1, 1);
    }

    compute_fence.reset();
    engine.compute_submit(compute_command_buffer, &compute_fence);
    compute_fence.wait();


    // std::vector<GICPReductor::GICPPartial> test_parcial_src;
    // test_parcial_src.resize(100);
    // for (int i = 0; i < test_parcial_src.size(); i++) {
    //     GICPReductor::GICPPartial parital{};

    //     for (int x = 0; x < 6; x++) {
    //         parital.g[x] = x + i;
    //         for (int y = 0; y < 6; y++) {
    //             parital.H[x][y] = x + y + i;
    //         }
    //     }
    //     parital.total_weighted_sq_error = i;
    //     parital.valid_count = 1;

    //     test_parcial_src[i] = parital;
    // }
    // // partial_src.upload(test_parcial_src.data(), test_parcial_src.size() * sizeof(GICPReductor::GICPPartial), 0);
    // partial_src.upload(test_parcial_src, 0);
    
    uint32_t partial_count = math_utils::div_up_u32(source_point_cloud.instance_count(), 32);
    // uint32_t partial_count =  math_utils::div_up_u32(test_parcial_src.size(), 32);
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

    glm::vec3 omega = glm::vec3(
        static_cast<float>(delta[0]),
        static_cast<float>(delta[1]),
        static_cast<float>(delta[2])
    );

    glm::vec3 v = glm::vec3(
        static_cast<float>(delta[3]),
        static_cast<float>(delta[4]),
        static_cast<float>(delta[5])
    );

    float omega_len = glm::length(omega);
    if (omega_len > max_rot) {
        omega *= max_rot / omega_len;
    }

    float v_len = glm::length(v);
    if (v_len > max_trans) {
        v *= max_trans / v_len;
    }

    // Quaternion update.
    // Matches matrix update: R_new = dR * R_old
    glm::quat dq = omega_to_quat(omega);

    source_point_cloud.transform.position += v;
    source_point_cloud.transform.rotation = glm::normalize(dq * source_point_cloud.transform.rotation);

    return rmse;
}

double GICPPass::fit(VoxelPointMap& voxel_point_map, PointCloud& source_point_cloud, VulkanBuffer& source_normal_buffer, uint32_t max_steps) {
    double rmse = 0;
    for (int i = 0; i < max_steps; i++) {
        rmse = step(voxel_point_map, source_point_cloud, source_normal_buffer);
        if (rmse < 0.5) {
            break;
        }
            
    }

    return rmse;
}

glm::quat GICPPass::omega_to_quat(const glm::vec3& omega) {
    float theta = glm::length(omega);

    if (theta < 1e-12f) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    glm::vec3 axis = omega / theta;
    return glm::normalize(glm::angleAxis(theta, axis));
}

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


glm::mat3 GICPPass::omega_to_mat3(const glm::vec3& omega) {
    float theta = glm::length(omega);

    if (theta < 1e-12f) {
        return glm::mat3(1.0f);
    }

    glm::vec3 axis = omega / theta;
    glm::mat4 R4 = glm::rotate(glm::mat4(1.0f), theta, axis);
    return glm::mat3(R4);
}

glm::vec3 GICPPass::mat3_to_euler_xyz(const glm::mat3& R) {
    float x, y, z;
    glm::extractEulerAngleXYZ(glm::mat4(R), x, y, z);
    return glm::vec3(x, y, z);
}