#include "gicp.h"

glm::mat3 GICP::euler_xyz_to_mat3(const glm::vec3& euler) {
    return glm::mat3(glm::eulerAngleXYZ(euler.x, euler.y, euler.z));
}


glm::vec3 GICP::transform_normal_world(const PointCloud& cloud, const glm::vec3& local_n) {
    glm::mat3 R = euler_xyz_to_mat3(cloud.rotation);

    glm::mat3 S(1.0f);
    S[0][0] = cloud.scale.x;
    S[1][1] = cloud.scale.y;
    S[2][2] = cloud.scale.z;

    glm::mat3 linear = R * S;
    glm::mat3 normal_matrix = glm::transpose(glm::inverse(linear));

    return glm::normalize(normal_matrix * local_n);
}

glm::vec3 GICP::transform_normal_world_test(const glm::vec3& cloud_rotation, const glm::vec3 cloud_scale, const glm::vec3& local_n) {
    glm::mat3 R = euler_xyz_to_mat3(cloud_rotation);

    glm::mat3 S(1.0f);
    S[0][0] = cloud_scale.x;
    S[1][1] = cloud_scale.y;
    S[2][2] = cloud_scale.z;

    glm::mat3 linear = R * S;
    glm::mat3 normal_matrix = glm::transpose(glm::inverse(linear));

    return glm::normalize(normal_matrix * local_n);
}

glm::vec3 GICP::transform_point_world(const PointCloud& cloud, const glm::vec3& local_p) {
    glm::mat3 R = euler_xyz_to_mat3(cloud.rotation);

    glm::vec3 scaled(
        local_p.x * cloud.scale.x,
        local_p.y * cloud.scale.y,
        local_p.z * cloud.scale.z
    );

    return R * scaled + cloud.position;
}


glm::vec3 GICP::transform_point_world_test(const glm::vec3& cloud_rotation, const glm::vec3& cloud_position, const glm::vec3 cloud_scale, const glm::vec3& local_p) {
    glm::mat3 R = euler_xyz_to_mat3(cloud_rotation);

    glm::vec3 scaled(
        local_p.x * cloud_scale.x,
        local_p.y * cloud_scale.y,
        local_p.z * cloud_scale.z
    );

    return R * scaled + cloud_position;
}

glm::mat3 GICP::covariance_from_normal(const glm::vec3& raw_n, float eps) {
    float len2 = glm::dot(raw_n, raw_n);
    if (len2 < 1e-12f) {
        return glm::mat3(0.0f);
    }

    glm::vec3 n = raw_n / std::sqrt(len2);
    glm::mat3 I(1.0f);
    glm::mat3 nnT = glm::outerProduct(n, n);

    // I - nn^T + eps * nn^T
    return I - (1.0f - eps) * nnT;
}

int GICP::find_closest_id_with_valid_normal(const std::vector<glm::vec3>& target_points_world,
                                    const std::vector<glm::vec3>& target_normals_world,
                                    const glm::vec3& point,
                                    float max_dist_sq)
{
    if (target_points_world.empty())
        return -1;

    int min_id = -1;
    float min_dist = max_dist_sq;

    for (size_t i = 0; i < target_points_world.size(); i++) {
        if (glm::dot(target_normals_world[i], target_normals_world[i]) < 1e-12f)
            continue;

        float dist = glm::distance2(target_points_world[i], point);
        if (dist < min_dist) {
            min_dist = dist;
            min_id = (int)i;
        }
    }

    return min_id;
}

glm::mat3 GICP::skew_matrix(const glm::vec3& v) {
    glm::mat3 S(0.0f);

    // GLM matrices are column-major: m[col][row]
    S[0] = glm::vec3( 0.0f,  v.z,  -v.y);
    S[1] = glm::vec3(-v.z,   0.0f,  v.x);
    S[2] = glm::vec3( v.y,  -v.x,   0.0f);

    return S;
}

void GICP::add_mat3_block(double H[6][6], int row0, int col0, const glm::mat3& M) {
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            H[row0 + r][col0 + c] += static_cast<double>(M[c][r]); // glm is [col][row]
        }
    }
}

void GICP::add_vec3_block(double g[6], int row0, const glm::vec3& v) {
    for (int r = 0; r < 3; r++) {
        g[row0 + r] += static_cast<double>(v[r]);
    }
}

// bool GICP::solve_6x6(const double H_in[6][6], const double g_in[6], double delta_out[6]) {
//     int counter = 0;
//     std::ofstream out("/home/spectre/Projects/test_open_3d/solve_6x6_cpu_dump.txt");


//     double a[6][7];

//     for (int r = 0; r < 6; r++) {
//         for (int c = 0; c < 6; c++) {
//             a[r][c] = H_in[r][c];
//         }
//         a[r][6] = g_in[r];
//     }

//     int counter = 0;

//     // Forward elimination with partial pivoting
//     for (int col = 0; col < 6; col++) {
//         // Find pivot row
//         int pivot_row = col;
//         double max_abs = std::abs(a[col][col]);

//         for (int r = col + 1; r < 6; r++) {
//             double v = std::abs(a[r][col]);
//             if (v > max_abs) {
//                 max_abs = v;
//                 pivot_row = r;
//             }
//         }

//         // Singular / degenerate check
//         if (max_abs < 1e-12) {
//             return false;
//         }

//         // Swap rows if needed
//         if (pivot_row != col) {
//             counter++;
//             for (int c = col; c < 7; c++) {
//                 std::swap(a[col][c], a[pivot_row][c]);
//             }
//         }

//         // Eliminate rows below
//         for (int r = col + 1; r < 6; r++) {
//             double factor = a[r][col] / a[col][col];

//             for (int c = col; c < 7; c++) {
//                 a[r][c] -= factor * a[col][c];
//             }
//         }
//     }


//     // Back substitution
//     for (int r = 5; r >= 0; r--) {
//         double sum = a[r][6];

//         for (int c = r + 1; c < 6; c++) {
//             sum -= a[r][c] * delta_out[c];
//         }

//         if (std::abs(a[r][r]) < 1e-12) {
//             return false;
//         }

//         delta_out[r] = sum / a[r][r];

//     }

//     return true;
// }

bool GICP::solve_6x6(const double H_in[6][6], const double g_in[6], double delta_out[6]) {
    int counter = 0;
    std::ofstream out("/home/spectre/Projects/test_open_3d/solve_6x6_cpu_dump.txt");
    out << std::setprecision(std::numeric_limits<double>::max_digits10);

    auto dump = [&](int ordinal, double value) {
        out << counter << "." << ordinal << " = " << value << '\n';
        counter++;
    };

    double a[6][7];

    for (int r = 0; r < 6; r++) {
        dump(1, static_cast<double>(r));

        for (int c = 0; c < 6; c++) {
            dump(2, static_cast<double>(c));

            a[r][c] = H_in[r][c];
            dump(3, a[r][c]);
        }

        a[r][6] = g_in[r];
        dump(4, a[r][6]);
    }

    // Forward elimination with partial pivoting
    for (int col = 0; col < 6; col++) {
        dump(5, static_cast<double>(col));

        // Find pivot row
        int pivot_row = col;
        dump(6, static_cast<double>(pivot_row));

        double max_abs = std::abs(a[col][col]);
        dump(7, max_abs);

        for (int r = col + 1; r < 6; r++) {
            dump(8, static_cast<double>(r));

            double v = std::abs(a[r][col]);
            dump(9, v);

            if (v > max_abs) {
                dump(10, 1.0); // entered if

                max_abs = v;
                dump(11, max_abs);

                pivot_row = r;
                dump(12, static_cast<double>(pivot_row));
            }
        }

        // Singular / degenerate check
        if (max_abs < 1e-12) {
            dump(13, 1.0); // entered if
            return false;
        }

        // Swap rows if needed
        if (pivot_row != col) {
            dump(14, 1.0); // entered if

            for (int c = col; c < 7; c++) {
                dump(15, static_cast<double>(c));

                std::swap(a[col][c], a[pivot_row][c]);

                dump(16, a[col][c]);
                dump(17, a[pivot_row][c]);
            }
        }

        // Eliminate rows below
        for (int r = col + 1; r < 6; r++) {
            dump(18, static_cast<double>(r));

            double factor = a[r][col] / a[col][col];
            dump(19, factor);

            for (int c = col; c < 7; c++) {
                dump(20, static_cast<double>(c));

                a[r][c] -= factor * a[col][c];
                dump(21, a[r][c]);
            }
        }
    }

    // Back substitution
    for (int r = 5; r >= 0; r--) {
        dump(22, static_cast<double>(r));

        double sum = a[r][6];
        dump(23, sum);

        for (int c = r + 1; c < 6; c++) {
            dump(24, static_cast<double>(c));

            sum -= a[r][c] * delta_out[c];
            dump(25, sum);
        }

        if (std::abs(a[r][r]) < 1e-12) {
            dump(26, 1.0); // entered if
            return false;
        }

        delta_out[r] = sum / a[r][r];
        dump(27, delta_out[r]);
    }

    dump(28, 1.0); // returning true
    return true;
}

glm::mat3 GICP::omega_to_mat3(const glm::vec3& omega) {
    float theta = glm::length(omega);

    if (theta < 1e-12f) {
        return glm::mat3(1.0f);
    }

    glm::vec3 axis = omega / theta;
    glm::mat4 R4 = glm::rotate(glm::mat4(1.0f), theta, axis);
    return glm::mat3(R4);
}

glm::vec3 GICP::mat3_to_euler_xyz(const glm::mat3& R) {
    float x, y, z;
    glm::extractEulerAngleXYZ(glm::mat4(R), x, y, z);
    return glm::vec3(x, y, z);
}

// double GICP::step(PointCloud& source_point_cloud,
//                 const PointCloud& target_point_cloud,
//                 const std::vector<glm::vec3>& source_normals,
//                 const std::vector<glm::vec3>& target_normals)
// {
//     const std::vector<PointInstance>& source_points = source_point_cloud.points;
//     const std::vector<PointInstance>& target_points = target_point_cloud.points;

//     if (source_points.size() != source_normals.size()) {
//         std::cout << "source_points.size() != source_normals.size()\n";
//         return -1.0;
//     }

//     if (target_points.size() != target_normals.size()) {
//         std::cout << "target_points.size() != target_normals.size()\n";
//         return -1.0;
//     }

//     float max_corr_dist = 5.0f;
//     float max_corr_dist_sq = max_corr_dist * max_corr_dist;
//     float max_rot = glm::radians(5.0f);
//     float max_trans = 5.0f;
//     float gicp_eps = 1e-3f;

//     double H[6][6] = {};
//     double g[6] = {};

//     // Precompute target points, normals, covariances in world space
//     std::vector<glm::vec3> target_points_world;
//     std::vector<glm::vec3> target_normals_world;
//     std::vector<glm::mat3> target_covs_world;

//     target_points_world.reserve(target_points.size());
//     target_normals_world.reserve(target_points.size());
//     target_covs_world.reserve(target_points.size());
    
//     std::vector<float> source_distances(source_points.size(), -1);
//     std::vector<float> target_indices(source_points.size(), -1);
//     float max_new_dist = 0;
//     // source_distances.reserve(source_points.size());

//     for (size_t i = 0; i < target_points.size(); i++) {
//         glm::vec3 q_local = glm::vec3(target_points[i].pos);
//         glm::vec3 n_world = transform_normal_world(target_point_cloud, target_normals[i]);

//         target_points_world.push_back(transform_point_world(target_point_cloud, q_local));
//         target_normals_world.push_back(n_world);
//         target_covs_world.push_back(covariance_from_normal(n_world, gicp_eps));
//     }

//     double total_weighted_sq_error = 0.0;
//     int valid_count = 0;

//     for (size_t i = 0; i < source_points.size(); i++) {
//         glm::vec3 p_local = glm::vec3(source_points[i].pos);

//         // Source point and source normal in world space
//         glm::vec3 x = transform_point_world(source_point_cloud, p_local);
//         glm::vec3 n_src_world = transform_normal_world(source_point_cloud, source_normals[i]);

//         if (glm::dot(n_src_world, n_src_world) < 1e-12f) {
//             continue;
//         }

//         int target_id = find_closest_id_with_valid_normal(
//             target_points_world,
//             target_normals_world,
//             x,
//             max_corr_dist_sq
//         );

//         if (target_id < 0) {
//             // source_point_cloud.points[i].color = glm::vec4(1, 0, 0, 1);
//             continue;
//         }
//         source_distances[i] = glm::distance(target_points_world[target_id], x);
//         if (source_distances[i] > max_new_dist)
//             max_new_dist = source_distances[i];

//         target_indices[i] = target_id;

//         // source_point_cloud.points[i].color = glm::vec4(0, 1, 0, 1);
//         source_point_cloud.points[i].color = glm::vec4(1, 0, 0, 1);

//         glm::vec3 q = target_points_world[target_id];
//         glm::vec3 n_tgt_world = target_normals_world[target_id];

//         if (glm::dot(n_tgt_world, n_tgt_world) < 1e-12f) {
//             continue;
//         }

//         // Build source and target local covariances
//         glm::mat3 C_A = covariance_from_normal(n_src_world, gicp_eps);
//         glm::mat3 C_B = target_covs_world[target_id];

//         // GICP weighting matrix:
//         // M = (C_B + C_A)^-1
//         // Since x and q are already in world space, both covariances are in world space too.
//         glm::mat3 Sigma = C_B + C_A;
//         glm::mat3 M = glm::inverse(Sigma);

//         glm::vec3 d = q - x;

//         // Linearization:
//         // x' ≈ x + ω × x + v
//         // d' = q - x' ≈ d + x×ω - v
//         // so A = [skew(x), -I], residual = d
//         glm::mat3 B = skew_matrix(x);
//         glm::mat3 I(1.0f);

//         glm::mat3 H00 = glm::transpose(B) * M * B;
//         glm::mat3 H01 = -glm::transpose(B) * M;
//         glm::mat3 H10 = -M * B;
//         glm::mat3 H11 = M;

//         glm::vec3 g0 = -glm::transpose(B) * M * d;
//         glm::vec3 g1 =  M * d;

//         add_mat3_block(H, 0, 0, H00);
//         add_mat3_block(H, 0, 3, H01);
//         add_mat3_block(H, 3, 0, H10);
//         add_mat3_block(H, 3, 3, H11);

//         add_vec3_block(g, 0, g0);
//         add_vec3_block(g, 3, g1);

//         total_weighted_sq_error += static_cast<double>(glm::dot(d, M * d));
//         valid_count++;
//     }

//     if (valid_count < 6) {
//         std::cout << "Too few correspondences\n";
//         return -1.0;
//     }

//     double rmse = std::sqrt(total_weighted_sq_error / static_cast<double>(valid_count));

//     double lambda = 1e-6;
//     for (int i = 0; i < 6; i++) {
//         H[i][i] += lambda;
//     }

//     double delta[6] = {};
//     bool ok = solve_6x6(H, g, delta);
//     if (!ok) {
//         std::cout << "solve_6x6 failed\n";
//         return -1.0;
//     }

//     glm::vec3 omega(
//         static_cast<float>(delta[0]),
//         static_cast<float>(delta[1]),
//         static_cast<float>(delta[2])
//     );

//     glm::vec3 v(
//         static_cast<float>(delta[3]),
//         static_cast<float>(delta[4]),
//         static_cast<float>(delta[5])
//     );

//     float omega_len = glm::length(omega);
//     if (omega_len > max_rot) {
//         omega *= max_rot / omega_len;
//     }

//     float v_len = glm::length(v);
//     if (v_len > max_trans) {
//         v *= max_trans / v_len;
//     }

//     glm::mat3 dR = omega_to_mat3(omega);

//     glm::mat3 R_src = euler_xyz_to_mat3(source_point_cloud.rotation);
//     glm::mat3 R_src_new = dR * R_src;
//     glm::vec3 t_src_new = dR * source_point_cloud.position + v;

//     source_point_cloud.position = t_src_new;
//     source_point_cloud.rotation = mat3_to_euler_xyz(R_src_new);
    
//     std::cout << "valid_count = " << valid_count
//             << ", weighted_rmse = " << rmse
//             << ", |omega| = " << glm::length(omega)
//             << ", |v| = " << glm::length(v)
//             << "\n";

//     return rmse;
// }



double GICP::step(PointCloud& source_point_cloud,
                  const PointCloud& target_point_cloud,
                  const std::vector<glm::vec4>& source_normals,
                  const std::vector<glm::vec4>& target_normals)
{
    const std::vector<PointInstance>& source_points = source_point_cloud.points;
    const std::vector<PointInstance>& target_points = target_point_cloud.points;

    if (source_points.size() != source_normals.size()) {
        std::cout << "source_points.size() != source_normals.size()\n";
        return -1.0;
    }

    if (target_points.size() != target_normals.size()) {
        std::cout << "target_points.size() != target_normals.size()\n";
        return -1.0;
    }

    float max_corr_dist = 5.0f;
    float max_corr_dist_sq = max_corr_dist * max_corr_dist;
    float max_rot = glm::radians(5.0f);
    float max_trans = 5.0f;
    float gicp_eps = 1e-3f;

    double H[6][6] = {};
    double g[6] = {};

    // Precompute target points, normals, covariances in world space
    std::vector<glm::vec3> target_points_world;
    std::vector<glm::vec3> target_normals_world;
    std::vector<glm::mat3> target_covs_world;

    target_points_world.reserve(target_points.size());
    target_normals_world.reserve(target_points.size());
    target_covs_world.reserve(target_points.size());

    std::vector<float> source_distances(source_points.size(), -1.0f);
    std::vector<float> target_indices(source_points.size(), -1.0f);
    float max_new_dist = 0.0f;

    for (size_t i = 0; i < target_points.size(); i++) {
        glm::vec3 q_local = glm::vec3(target_points[i].pos);

        // Convert vec4 normal -> vec3 normal (ignore w)
        glm::vec3 n_local = glm::vec3(target_normals[i]);
        glm::vec3 n_world = transform_normal_world(target_point_cloud, n_local);

        target_points_world.push_back(transform_point_world(target_point_cloud, q_local));
        target_normals_world.push_back(n_world);
        target_covs_world.push_back(covariance_from_normal(n_world, gicp_eps));
    }

    double total_weighted_sq_error = 0.0;
    int valid_count = 0;

    for (size_t i = 0; i < source_points.size(); i++) {
        glm::vec3 p_local = glm::vec3(source_points[i].pos);

        // Convert vec4 normal -> vec3 normal (ignore w)
        glm::vec3 n_src_local = glm::vec3(source_normals[i]);

        // Source point and source normal in world space
        glm::vec3 x = transform_point_world(source_point_cloud, p_local);
        glm::vec3 n_src_world = transform_normal_world(source_point_cloud, n_src_local);

        if (glm::dot(n_src_world, n_src_world) < 1e-12f) {
            continue;
        }

        int target_id = find_closest_id_with_valid_normal(
            target_points_world,
            target_normals_world,
            x,
            max_corr_dist_sq
        );

        if (target_id < 0) {
            continue;
        }

        source_distances[i] = glm::distance(target_points_world[target_id], x);
        if (source_distances[i] > max_new_dist) {
            max_new_dist = source_distances[i];
        }

        target_indices[i] = static_cast<float>(target_id);

        source_point_cloud.points[i].color = glm::vec4(1, 0, 0, 1);

        glm::vec3 q = target_points_world[target_id];
        glm::vec3 n_tgt_world = target_normals_world[target_id];

        if (glm::dot(n_tgt_world, n_tgt_world) < 1e-12f) {
            continue;
        }

        // Build source and target local covariances
        glm::mat3 C_A = covariance_from_normal(n_src_world, gicp_eps);
        glm::mat3 C_B = target_covs_world[target_id];

        // GICP weighting matrix:
        // M = (C_B + C_A)^-1
        glm::mat3 Sigma = C_B + C_A;
        glm::mat3 M = glm::inverse(Sigma);

        glm::vec3 d = q - x;

        // Linearization:
        // x' ≈ x + ω × x + v
        // d' = q - x' ≈ d + x×ω - v
        // so A = [skew(x), -I], residual = d
        glm::mat3 B = skew_matrix(x);

        glm::mat3 H00 = glm::transpose(B) * M * B;
        glm::mat3 H01 = -glm::transpose(B) * M;
        glm::mat3 H10 = -M * B;
        glm::mat3 H11 = M;

        glm::vec3 g0 = -glm::transpose(B) * M * d;
        glm::vec3 g1 = M * d;

        add_mat3_block(H, 0, 0, H00);
        add_mat3_block(H, 0, 3, H01);
        add_mat3_block(H, 3, 0, H10);
        add_mat3_block(H, 3, 3, H11);

        add_vec3_block(g, 0, g0);
        add_vec3_block(g, 3, g1);

        total_weighted_sq_error += static_cast<double>(glm::dot(d, M * d));
        valid_count++;
    }

    if (valid_count < 6) {
        std::cout << "Too few correspondences\n";
        return -1.0;
    }

    double rmse = std::sqrt(total_weighted_sq_error / static_cast<double>(valid_count));

    double lambda = 1e-6;
    for (int i = 0; i < 6; i++) {
        H[i][i] += lambda;
    }

    double delta[6] = {};
    bool ok = solve_6x6(H, g, delta);
    if (!ok) {
        std::cout << "solve_6x6 failed\n";
        return -1.0;
    }

    glm::vec3 omega(
        static_cast<float>(delta[0]),
        static_cast<float>(delta[1]),
        static_cast<float>(delta[2])
    );

    glm::vec3 v(
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

    glm::mat3 dR = omega_to_mat3(omega);

    glm::mat3 R_src = euler_xyz_to_mat3(source_point_cloud.rotation);
    glm::mat3 R_src_new = dR * R_src;
    glm::vec3 t_src_new = dR * source_point_cloud.position + v;

    source_point_cloud.position = t_src_new;
    source_point_cloud.rotation = mat3_to_euler_xyz(R_src_new);

    std::cout << "valid_count = " << valid_count
              << ", weighted_rmse = " << rmse
              << ", |omega| = " << glm::length(omega)
              << ", |v| = " << glm::length(v)
              << "\n";

    return rmse;
}

double GICP::step_test(PointCloud& source_point_cloud,
                  const PointCloud& target_point_cloud,
                  const std::vector<glm::vec4>& source_normals,
                  const std::vector<glm::vec4>& target_normals)
{
    const auto& source_points = source_point_cloud.points;
    const auto& target_points = target_point_cloud.points;   // already in world space
    const auto& src_normals   = source_normals;
    const auto& tgt_normals   = target_normals;              // already in world space

    if (source_points.size() != src_normals.size()) {
        std::cout << "source_points.size() != source_normals.size()\n";
        return -1.0;
    }

    if (target_points.size() != tgt_normals.size()) {
        std::cout << "target_points.size() != target_normals.size()\n";
        return -1.0;
    }

    const float max_corr_dist    = 10.0f;
    const float max_corr_dist_sq = max_corr_dist * max_corr_dist;
    const float max_rot          = glm::radians(5.0f);
    const float max_trans        = 5.0f;
    const float gicp_eps         = 1e-3f;
    const float min_normal_dot   = -1.0f; // currently effectively disabled

    double H[6][6] = {};
    double g[6]    = {};

    double total_weighted_sq_error = 0.0;
    int valid_count = 0;

    // const glm::vec3 source_rotation = source_location.rotation;
    // const glm::vec3 source_scale(1.0f, 1.0f, 1.0f);
    // const glm::vec3 source_position = source_location.position;

    for (size_t i = 0; i < source_points.size(); ++i) {
        // Replace .position with .pos if that is your actual field name.
        glm::vec3 p_local = glm::vec3(source_points[i].pos);

        // Replace .normal access if your normal storage is different.
        glm::vec3 n_src_local = glm::vec3(src_normals[i]);

        glm::vec3 x = transform_point_world(
            source_point_cloud,
            p_local
        );

        glm::vec3 n_src_world = transform_normal_world(
            source_point_cloud,
            n_src_local
        );

        if (glm::dot(n_src_world, n_src_world) < 1e-12f) {
            continue;
        }

        // Brute-force closest target search directly on world-space target data.
        int target_id = -1;
        float best_dist_sq = max_corr_dist_sq;

        for (size_t j = 0; j < target_points.size(); ++j) {
            glm::vec3 n_tgt_world = glm::vec3(tgt_normals[j]);

            if (glm::dot(n_tgt_world, n_tgt_world) < 1e-12f) {
                continue;
            }

            glm::vec3 q = glm::vec3(target_points[j].pos); // or .pos
            glm::vec3 diff = q - x;
            float dist_sq = glm::dot(diff, diff);

            if (dist_sq >= best_dist_sq) {
                continue;
            }

            if (min_normal_dot > -1.0f) {
                float src_len = glm::length(n_src_world);
                float tgt_len = glm::length(n_tgt_world);
                if (src_len < 1e-12f || tgt_len < 1e-12f) {
                    continue;
                }

                float ndot = glm::dot(n_src_world / src_len, n_tgt_world / tgt_len);
                if (ndot < min_normal_dot) {
                    continue;
                }
            }

            best_dist_sq = dist_sq;
            target_id = static_cast<int>(j);
        }



        if (target_id < 0) {
            continue;
        }

        glm::vec3 q = glm::vec3(target_points[target_id].pos); // or .pos
        glm::vec3 n_tgt_world = glm::vec3(tgt_normals[target_id]);

        glm::mat3 C_A = covariance_from_normal(n_src_world, gicp_eps);
        glm::mat3 C_B = covariance_from_normal(n_tgt_world, gicp_eps);

        glm::mat3 Sigma = C_B + C_A;
        glm::mat3 M = glm::inverse(Sigma);

        glm::vec3 d = q - x;

        glm::mat3 B = skew_matrix(x);

        glm::mat3 H00 = glm::transpose(B) * M * B;
        glm::mat3 H01 = -glm::transpose(B) * M;
        glm::mat3 H10 = -M * B;
        glm::mat3 H11 = M;

        glm::vec3 g0 = -glm::transpose(B) * M * d;
        glm::vec3 g1 = M * d;

        add_mat3_block(H, 0, 0, H00);
        add_mat3_block(H, 0, 3, H01);
        add_mat3_block(H, 3, 0, H10);
        add_mat3_block(H, 3, 3, H11);

        add_vec3_block(g, 0, g0);
        add_vec3_block(g, 3, g1);

        total_weighted_sq_error += glm::dot(d, M * d);
        valid_count++;
    }

    if (valid_count < 6) {
        return -1.0;
    }

    double rmse = std::sqrt(total_weighted_sq_error / double(valid_count));

    const double lambda = 1e-6;
    for (int i = 0; i < 6; ++i) {
        H[i][i] += lambda;
    }

    double delta[6] = {};

    bool ok = solve_6x6(H, g, delta);
    if (!ok) {
        return -1.0;
    }

    glm::vec3 omega(
        (float)delta[0],
        (float)delta[1],
        (float)delta[2]
    );

    glm::vec3 v(
        (float)delta[3],
        (float)delta[4],
        (float)delta[5]
    );

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
    glm::vec3 t_src_new = dR * source_point_cloud.position + v;

    source_point_cloud.position = t_src_new;
    source_point_cloud.rotation = mat3_to_euler_xyz(R_src_new);

    return rmse;
}

void print_H_and_g(const double H[6][6], const double g[6]) {
    std::cout << std::setprecision(std::numeric_limits<double>::max_digits10);

    std::cout << "double H[6][6] = double[6][6](\n";
    for (int i = 0; i < 6; ++i) {
        std::cout << "    double[6](";
        for (int j = 0; j < 6; ++j) {
            std::cout << H[i][j];
            if (j < 5) std::cout << ", ";
        }
        std::cout << ")";
        if (i < 5) std::cout << ",";
        std::cout << "\n";
    }
    std::cout << ");\n\n";

    std::cout << "double g[6] = double[6](";
    for (int i = 0; i < 6; ++i) {
        std::cout << g[i];
        if (i < 5) std::cout << ", ";
    }
    std::cout << ");\n";
}

void print_delta(const double delta[6]) {
    std::cout << std::setprecision(std::numeric_limits<double>::max_digits10);

    std::cout << "double delta[6] = {";
    for (int i = 0; i < 6; ++i) {
        std::cout << delta[i];
        if (i < 5) std::cout << ", ";
    }
    std::cout << "};\n";
}






double GICP::step_test2(std::vector<PointInstance>& source_points,
                        const std::vector<PointInstance>& target_points,
                        const std::vector<glm::vec4>& source_normals,
                        const std::vector<glm::vec4>& target_normals, glm::vec4& source_position, glm::vec4& source_rotation, std::vector<uint32_t>& target_ids, std::vector<OutputHG>& output_hg_cpu)
{
    // const auto& source_points = source_points;
    // const auto& target_points = target_points;   // already in world space
    const auto& src_normals   = source_normals;
    const auto& tgt_normals   = target_normals;              // already in world space

    glm::vec3 source_scale = glm::vec3(1.0f, 1.0f, 1.0f);

    if (source_points.size() != src_normals.size()) {
        std::cout << "source_points.size() != source_normals.size()\n";
        return -1.0;
    }

    if (target_points.size() != tgt_normals.size()) {
        std::cout << "target_points.size() != target_normals.size()\n";
        return -1.0;
    }

    const float max_corr_dist    = 10.0f;
    const float max_corr_dist_sq = max_corr_dist * max_corr_dist;
    const float max_rot          = glm::radians(5.0f);
    const float max_trans        = 5.0f;
    const float gicp_eps         = 1e-3f;
    const float min_normal_dot   = -1.0f; // currently effectively disabled

    double H[6][6] = {};
    double g[6]    = {};

    double total_weighted_sq_error = 0.0;
    int valid_count = 0;

    // const glm::vec3 source_rotation = source_location.rotation;
    // const glm::vec3 source_scale(1.0f, 1.0f, 1.0f);
    // const glm::vec3 source_position = source_location.position;

    for (size_t i = 0; i < source_points.size(); ++i) {
        // Replace .position with .pos if that is your actual field name.
        glm::vec3 p_local = glm::vec3(source_points[i].pos);

        // Replace .normal access if your normal storage is different.
        glm::vec3 n_src_local = glm::vec3(src_normals[i]);

        glm::vec3 x = transform_point_world_test(source_rotation, source_position, source_scale,
            p_local
        );

        glm::vec3 n_src_world = transform_normal_world_test(source_rotation, source_scale, n_src_local);

        if (glm::dot(n_src_world, n_src_world) < 1e-12f) {
            continue;
        }

        // Brute-force closest target search directly on world-space target data.
        int target_id = -1;
        float best_dist_sq = max_corr_dist_sq;

        for (size_t j = 0; j < target_points.size(); ++j) {
            glm::vec3 n_tgt_world = glm::vec3(tgt_normals[j]);

            if (glm::dot(n_tgt_world, n_tgt_world) < 1e-12f) {
                continue;
            }

            glm::vec3 q = glm::vec3(target_points[j].pos); // or .pos
            glm::vec3 diff = q - x;
            float dist_sq = glm::dot(diff, diff);

            if (dist_sq >= best_dist_sq) {
                continue;
            }

            if (min_normal_dot > -1.0f) {
                float src_len = glm::length(n_src_world);
                float tgt_len = glm::length(n_tgt_world);
                if (src_len < 1e-12f || tgt_len < 1e-12f) {
                    continue;
                }

                float ndot = glm::dot(n_src_world / src_len, n_tgt_world / tgt_len);
                if (ndot < min_normal_dot) {
                    continue;
                }
            }

            best_dist_sq = dist_sq;
            target_id = static_cast<int>(j);
        }

        if (target_id < 0) {
            continue;
        }

        target_ids[i] = target_id;

        glm::vec3 q = glm::vec3(target_points[target_id].pos); // or .pos
        glm::vec3 n_tgt_world = glm::vec3(tgt_normals[target_id]);

        glm::mat3 C_A = covariance_from_normal(n_src_world, gicp_eps);
        glm::mat3 C_B = covariance_from_normal(n_tgt_world, gicp_eps);

        glm::mat3 Sigma = C_B + C_A;
        glm::mat3 M = glm::inverse(Sigma);

        glm::vec3 d = q - x;

        glm::mat3 B = skew_matrix(x);

        glm::mat3 H00 = glm::transpose(B) * M * B;
        glm::mat3 H01 = -glm::transpose(B) * M;
        glm::mat3 H10 = -M * B;
        glm::mat3 H11 = M;

        glm::vec3 g0 = -glm::transpose(B) * M * d;
        glm::vec3 g1 = M * d;

        add_mat3_block(H, 0, 0, H00);
        add_mat3_block(H, 0, 3, H01);
        add_mat3_block(H, 3, 0, H10);
        add_mat3_block(H, 3, 3, H11);

        add_vec3_block(g, 0, g0);
        add_vec3_block(g, 3, g1);

        // output_hg_cpu[i].h = H;
        // output_hg_cpu[i].g = g;

        
            

        

        total_weighted_sq_error += glm::dot(d, M * d);

        if (i == 0)
            std::cout << total_weighted_sq_error << std::endl;
        valid_count++;
    }

    // std::cout << total_weighted_sq_error << std::endl;

    // if (valid_count < 6) {
    //     return -1.0;
    // }

    double rmse = std::sqrt(total_weighted_sq_error / double(valid_count));

    std::cout << rmse << std::endl;

    const double lambda = 1e-6;
    for (int i = 0; i < 6; ++i) {
        H[i][i] += lambda;
    }

    double delta[6] = {};

    for (int a = 0; a < 6; a++) {
        output_hg_cpu[0].g[a] = g[a];
        for (int b = 0; b < 6; b++) {
            output_hg_cpu[0].h[a][b] = H[a][b];
        }
    }
    
    // print_H_and_g(H, g);

    bool ok = solve_6x6(H, g, delta);

    // std::cout << "bool ok = " << ok << std::endl;

    // print_delta(delta);


    if (!ok) {
        return -1.0;
    }

    glm::vec3 omega(
        (float)delta[0],
        (float)delta[1],
        (float)delta[2]
    );

    glm::vec3 v(
        (float)delta[3],
        (float)delta[4],
        (float)delta[5]
    );

    float omega_len = glm::length(omega);
    if (omega_len > max_rot) {
        omega *= max_rot / omega_len;
    }

    float v_len = glm::length(v);
    if (v_len > max_trans) {
        v *= max_trans / v_len;
    }

    glm::mat3 dR = omega_to_mat3(omega);

    glm::mat3 R_src = euler_xyz_to_mat3(source_rotation);
    glm::mat3 R_src_new = dR * R_src;
    glm::vec3 t_src_new = dR * source_position + v;

    // std::cout << "t_src_new: (" << t_src_new.x << ", " << t_src_new.y << ", " << t_src_new.z << ")" << std::endl;
    std::cout << "delta3-5: (" << delta[3] << ", " << delta[4] << ", " << delta[5] << ")" << std::endl;

    source_position = glm::vec4(t_src_new, 1.0f);
    source_rotation = glm::vec4(mat3_to_euler_xyz(R_src_new), 1.0f);

    // std::cout << "position: (" << source_position.x << ", " << source_position.y << ", " << source_position.z << ")" << std::endl;
    // std::cout << "rotation: (" << source_rotation.x << ", " << source_rotation.y << ", " << source_rotation.z << ")" << std::endl;


    return rmse;
}


double GICP::align_gpu(PointCloud& source_point_cloud,
                        const PointCloud& target_point_cloud,
                        const std::vector<glm::vec3>& source_normals,
                        const std::vector<glm::vec3>& target_normals) {

    
}