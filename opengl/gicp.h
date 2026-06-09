#pragma once
#include "point_cloud/point_cloud.h"

struct OutputHG {
    double h[6][6];
    double g[6];
};

class GICP {
public:
    template <size_t M, size_t N>
    static void print_mxn(const std::string& name, const double (&matrix)[M][N]) {
        std::cout << std::setprecision(std::numeric_limits<double>::max_digits10);

        std::cout << "double " << name << "[" << M << "][" << N << "] = {\n";
        for (size_t i = 0; i < M; ++i) {
            std::cout << "    {";
            for (size_t j = 0; j < N; ++j) {
                std::cout << matrix[i][j];
                if (j + 1 < N) std::cout << ", ";
            }
            std::cout << "}";
            if (i + 1 < M) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "};\n";
    }


    static glm::mat3 euler_xyz_to_mat3(const glm::vec3& euler);
    static glm::vec3 transform_normal_world(const PointCloud& cloud, const glm::vec3& local_n);
    static glm::vec3 transform_normal_world_test(const glm::vec3& cloud_rotation, const glm::vec3 cloud_scale, const glm::vec3& local_n);
    static glm::vec3 transform_point_world(const PointCloud& cloud, const glm::vec3& local_p);
    static glm::vec3 transform_point_world_test(const glm::vec3& cloud_rotation, const glm::vec3& cloud_position, const glm::vec3 cloud_scale, const glm::vec3& local_p);
    static glm::mat3 covariance_from_normal(const glm::vec3& raw_n, float eps = 1e-3f);
    static int find_closest_id_with_valid_normal(const std::vector<glm::vec3>& target_points_world,
                                      const std::vector<glm::vec3>& target_normals_world,
                                      const glm::vec3& point,
                                      float max_dist_sq);
    static glm::mat3 skew_matrix(const glm::vec3& v);
    static void add_mat3_block(double H[6][6], int row0, int col0, const glm::mat3& M);
    static void add_vec3_block(double g[6], int row0, const glm::vec3& v);
    static bool solve_6x6(const double H_in[6][6], const double g_in[6], double delta_out[6]);
    static glm::mat3 omega_to_mat3(const glm::vec3& omega);
    static glm::vec3 mat3_to_euler_xyz(const glm::mat3& R);
    static double step(PointCloud& source_point_cloud,
                    const PointCloud& target_point_cloud,
                    const std::vector<glm::vec4>& source_normals,
                    const std::vector<glm::vec4>& target_normals);
    static double step_test(PointCloud& source_point_cloud,
                    const PointCloud& target_point_cloud,
                    const std::vector<glm::vec4>& source_normals,
                    const std::vector<glm::vec4>& target_normals);
    static double step_test2(std::vector<PointInstance>& source_points,
                        const std::vector<PointInstance>& target_points,
                        const std::vector<glm::vec4>& source_normals,
                        const std::vector<glm::vec4>& target_normals, glm::vec4& source_position, glm::vec4& source_rotation, std::vector<uint32_t>& target_ids, std::vector<OutputHG>& output_hg_cpu);
    static double align_gpu(PointCloud& source_point_cloud,
                            const PointCloud& target_point_cloud,
                            const std::vector<glm::vec3>& source_normals,
                            const std::vector<glm::vec3>& target_normals);
};