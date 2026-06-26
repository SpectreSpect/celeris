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



struct VoxelKey {
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const VoxelKey& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct VoxelKeyHash {
    std::size_t operator()(const VoxelKey& k) const {
        // Simple integer hash combine
        std::size_t h1 = std::hash<int>{}(k.x);
        std::size_t h2 = std::hash<int>{}(k.y);
        std::size_t h3 = std::hash<int>{}(k.z);

        std::size_t seed = h1;
        seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};

struct VoxelCell {
    glm::vec3 sum_pos{0.0f};
    glm::vec3 sum_normal{0.0f};

    std::uint32_t point_count = 0;
    std::uint32_t normal_count = 0;

    void add_point(const glm::vec3& p) {
        sum_pos += p;
        point_count++;
    }

    void add_normal(const glm::vec3& n) {
        float len2 = glm::dot(n, n);
        if (len2 < 1e-12f)
            return;

        sum_normal += glm::normalize(n);
        normal_count++;
    }

    void add(const glm::vec3& p) {
        add_point(p);
    }

    void add(const glm::vec3& p, const glm::vec3& n) {
        add_point(p);
        add_normal(n);
    }

    glm::vec3 centroid() const {
        if (point_count == 0)
            return glm::vec3(0.0f);

        return sum_pos / static_cast<float>(point_count);
    }

    glm::vec3 average_normal() const {
        if (normal_count == 0)
            return glm::vec3(0.0f);

        float len2 = glm::dot(sum_normal, sum_normal);
        if (len2 < 1e-12f)
            return glm::vec3(0.0f);

        return glm::normalize(sum_normal);
    }
};

class VoxelMap {
public:
    explicit VoxelMap(float voxel_size)
        : voxel_size_(std::max(voxel_size, 1e-6f)),
          inv_voxel_size_(1.0f / voxel_size_) {}

    void clear() {
        cells_.clear();
    }

    std::size_t voxel_count() const {
        return cells_.size();
    }

    float voxel_size() const {
        return voxel_size_;
    }

    VoxelKey point_to_key(const glm::vec3& p) const {
        return VoxelKey{
            static_cast<int>(std::floor(p.x * inv_voxel_size_)),
            static_cast<int>(std::floor(p.y * inv_voxel_size_)),
            static_cast<int>(std::floor(p.z * inv_voxel_size_))
        };
    }

    glm::vec3 key_to_center(const VoxelKey& k) const {
        return glm::vec3(
            (static_cast<float>(k.x) + 0.5f) * voxel_size_,
            (static_cast<float>(k.y) + 0.5f) * voxel_size_,
            (static_cast<float>(k.z) + 0.5f) * voxel_size_
        );
    }

    void insert_point(const glm::vec3& world_p) {
        VoxelKey key = point_to_key(world_p);
        cells_[key].add(world_p);
    }

    void insert_point(const glm::vec3& world_p, const glm::vec3& world_n) {
        VoxelKey key = point_to_key(world_p);
        cells_[key].add(world_p, world_n);
    }

    void insert_points(const std::vector<glm::vec3>& world_points) {
        for (const glm::vec3& p : world_points) {
            insert_point(p);
        }
    }

    void insert_points(const std::vector<glm::vec3>& world_points,
                       const std::vector<glm::vec3>& world_normals) {
        if (world_points.size() != world_normals.size())
            return;

        for (std::size_t i = 0; i < world_points.size(); i++) {
            insert_point(world_points[i], world_normals[i]);
        }
    }

    std::vector<glm::vec3> extract_centroids() const {
        std::vector<glm::vec3> out;
        out.reserve(cells_.size());

        for (const auto& [key, cell] : cells_) {
            if (cell.point_count == 0)
                continue;

            out.push_back(cell.centroid());
        }

        return out;
    }

    void extract_centroids_and_normals(std::vector<glm::vec3>& points,
                                       std::vector<glm::vec3>& normals) const {
        points.clear();
        normals.clear();

        points.reserve(cells_.size());
        normals.reserve(cells_.size());

        for (const auto& [key, cell] : cells_) {
            if (cell.point_count == 0)
                continue;

            points.push_back(cell.centroid());
            normals.push_back(cell.average_normal());
        }
    }

    void extract_local_centroids_and_normals(const glm::vec3& center,
                                             float radius,
                                             std::vector<glm::vec3>& points,
                                             std::vector<glm::vec3>& normals) const {
        points.clear();
        normals.clear();

        float radius_sq = radius * radius;

        for (const auto& [key, cell] : cells_) {
            if (cell.point_count == 0)
                continue;

            glm::vec3 c = cell.centroid();
            float d2 = glm::distance2(c, center);
            if (d2 > radius_sq)
                continue;

            points.push_back(c);
            normals.push_back(cell.average_normal());
        }
    }

    std::vector<PointInstance> extract_point_instances(const glm::vec4& color = glm::vec4(1, 1, 1, 1)) const {
        std::vector<PointInstance> out;
        out.reserve(cells_.size());

        for (const auto& [key, cell] : cells_) {
            if (cell.point_count == 0)
                continue;

            PointInstance p;
            p.pos = glm::vec4(cell.centroid(), 1.0f);
            p.color = color;
            out.push_back(p);
        }

        return out;
    }

private:
    float voxel_size_;
    float inv_voxel_size_;

    std::unordered_map<VoxelKey, VoxelCell, VoxelKeyHash> cells_;
};

// float clear_col[4] = {0.15f, 0.15f, 0.18f, 1.0f};
// float clear_col[4] = {0.1f, 0.1f, 0.1f, 1.0f};
float clear_col[4] = {0.1f, 0.1f, 0.1f, 1.0f};

void set_light_position(SSBO& light_source_ssbo, size_t index, const glm::vec4 &pos) {
    size_t offset = index * sizeof(LightSource);
    light_source_ssbo.update_subdata(offset, &pos, sizeof(pos));
}

int find_closest_id(const std::vector<PointInstance>& target_points,
                    const glm::vec3& point,
                    float max_dist_sq)
{
    if (target_points.empty())
        return -1;

    int min_id = -1;
    float min_dist = max_dist_sq;

    for (size_t i = 0; i < target_points.size(); i++) {
        float dist = glm::distance2(glm::vec3(target_points[i].pos), point);
        if (dist < min_dist) {
            min_dist = dist;
            min_id = (int)i;
        }
    }

    return min_id;
}


bool solve_6x6(const double H_in[6][6], const double g_in[6], double delta_out[6]) {
    // Augmented matrix [H | g], size 6 x 7
    double a[6][7];

    for (int r = 0; r < 6; r++) {
        for (int c = 0; c < 6; c++) {
            a[r][c] = H_in[r][c];
        }
        a[r][6] = g_in[r];
    }

    // Forward elimination with partial pivoting
    for (int col = 0; col < 6; col++) {
        // Find pivot row
        int pivot_row = col;
        double max_abs = std::abs(a[col][col]);

        for (int r = col + 1; r < 6; r++) {
            double v = std::abs(a[r][col]);
            if (v > max_abs) {
                max_abs = v;
                pivot_row = r;
            }
        }

        // Singular / degenerate check
        if (max_abs < 1e-12) {
            return false;
        }

        // Swap rows if needed
        if (pivot_row != col) {
            for (int c = col; c < 7; c++) {
                std::swap(a[col][c], a[pivot_row][c]);
            }
        }

        // Eliminate rows below
        for (int r = col + 1; r < 6; r++) {
            double factor = a[r][col] / a[col][col];

            for (int c = col; c < 7; c++) {
                a[r][c] -= factor * a[col][c];
            }
        }
    }

    // Back substitution
    for (int r = 5; r >= 0; r--) {
        double sum = a[r][6];

        for (int c = r + 1; c < 6; c++) {
            sum -= a[r][c] * delta_out[c];
        }

        if (std::abs(a[r][r]) < 1e-12) {
            return false;
        }

        delta_out[r] = sum / a[r][r];
    }

    return true;
}

glm::mat3 omega_to_mat3(const glm::vec3& omega) {
    float theta = glm::length(omega);

    if (theta < 1e-12f) {
        return glm::mat3(1.0f);
    }

    glm::vec3 axis = omega / theta;
    glm::mat4 R4 = glm::rotate(glm::mat4(1.0f), theta, axis);
    return glm::mat3(R4);
}

void icp_step(const std::vector<PointInstance>& source_points, 
              const std::vector<PointInstance>& target_points, const std::vector<glm::vec3>& target_normals, 
              glm::mat3& R, glm::vec3& t) { 

    float max_corr_dist = 20.0f;
    float max_corr_dist_sq = max_corr_dist * max_corr_dist;
    float max_rot = glm::radians(5.0f);
    float max_trans = 5.0f;

    double H[6][6] = {};
    double g[6] = {};

    double total_sq_error = 0.0;
    int valid_count = 0;
    for (size_t i = 0; i < source_points.size(); i++) {
        // int target_id = correspondences[i];
        glm::vec3 p = glm::vec3(source_points[i].pos);
        glm::vec3 x = R * p + t;
        
        int target_id = find_closest_id(target_points, x, max_corr_dist_sq);
        if (target_id < 0) {
            continue;
        }
        // int target_id = (int)i;
        
        glm::vec3 q = glm::vec3(target_points[target_id].pos);
        glm::vec3 n = glm::normalize(target_normals[target_id]);

        glm::vec3 a = glm::cross(x, n);
        double rhs = (double)glm::dot(n, q - x);

        total_sq_error += rhs * rhs;

        double J[6] = {
            (double)a.x,
            (double)a.y,
            (double)a.z,
            (double)n.x,
            (double)n.y,
            (double)n.z
        };

        for (int r = 0; r < 6; r++) {
            g[r] += J[r] * rhs;

            for (int c = 0; c < 6; c++) {
                H[r][c] += J[r] * J[c];
            }
        }

        valid_count++;
    }

    if (valid_count < 6) {
        std::cout << "Too few correspondences\n";
        return;
    }

    double rmse = std::sqrt(total_sq_error / (double)valid_count);

    // damping
    double lambda = 1e-6;
    for (int i = 0; i < 6; i++) {
        H[i][i] += lambda;
    }

    double delta[6] = {};
    bool ok = solve_6x6(H, g, delta);

    if (!ok) {
        std::cout << "solve_6x6 failed\n";
        return;
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

    R = dR * R;
    t = dR * t + v;

    std::cout << "valid_count = " << valid_count
          << ", rmse = " << rmse
          << ", |omega| = " << glm::length(omega)
          << ", |v| = " << glm::length(v)
          << "\n";
}

// Same order everywhere: XYZ
glm::mat3 euler_xyz_to_mat3(const glm::vec3& euler) {
    return glm::mat3(glm::eulerAngleXYZ(euler.x, euler.y, euler.z));
}

glm::vec3 mat3_to_euler_xyz(const glm::mat3& R) {
    float x, y, z;
    glm::extractEulerAngleXYZ(glm::mat4(R), x, y, z);
    return glm::vec3(x, y, z);
}

glm::vec3 transform_point_world(const PointCloud& cloud, const glm::vec3& local_p) {
    glm::mat3 R = euler_xyz_to_mat3(cloud.rotation);

    glm::vec3 scaled(
        local_p.x * cloud.scale.x,
        local_p.y * cloud.scale.y,
        local_p.z * cloud.scale.z
    );

    return R * scaled + cloud.position;
}

// Correct normal transform for non-uniform scale too
glm::vec3 transform_normal_world(const PointCloud& cloud, const glm::vec3& local_n) {
    glm::mat3 R = euler_xyz_to_mat3(cloud.rotation);

    glm::mat3 S(1.0f);
    S[0][0] = cloud.scale.x;
    S[1][1] = cloud.scale.y;
    S[2][2] = cloud.scale.z;

    glm::mat3 linear = R * S;
    glm::mat3 normal_matrix = glm::transpose(glm::inverse(linear));

    return glm::normalize(normal_matrix * local_n);
}

int find_closest_id(const std::vector<glm::vec3>& target_points_world,
                    const glm::vec3& point,
                    float max_dist_sq)
{
    if (target_points_world.empty())
        return -1;

    int min_id = -1;
    float min_dist = max_dist_sq;

    for (size_t i = 0; i < target_points_world.size(); i++) {
        float dist = glm::distance2(target_points_world[i], point);
        if (dist < min_dist) {
            min_dist = dist;
            min_id = (int)i;
        }
    }

    return min_id;
}

int find_closest_id_with_valid_normal(const std::vector<glm::vec3>& target_points_world,
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

double icp_step(PointCloud& source_point_cloud,
              const PointCloud& target_point_cloud,
              const std::vector<glm::vec3>& target_normals)
{
    const std::vector<PointInstance>& source_points = source_point_cloud.points;
    const std::vector<PointInstance>& target_points = target_point_cloud.points;

    if (target_points.size() != target_normals.size()) {
        std::cout << "target_points.size() != target_normals.size()\n";
        return -1;
    }

    float max_corr_dist = 20.0f;
    float max_corr_dist_sq = max_corr_dist * max_corr_dist;
    float max_rot = glm::radians(5.0f);
    float max_trans = 5.0f;

    double H[6][6] = {};
    double g[6] = {};

    // Precompute target in world space
    std::vector<glm::vec3> target_points_world;
    std::vector<glm::vec3> target_normals_world;
    target_points_world.reserve(target_points.size());
    target_normals_world.reserve(target_points.size());

    for (size_t i = 0; i < target_points.size(); i++) {
        glm::vec3 q_local = glm::vec3(target_points[i].pos);
        target_points_world.push_back(transform_point_world(target_point_cloud, q_local));
        target_normals_world.push_back(transform_normal_world(target_point_cloud, target_normals[i]));
    }

    double total_sq_error = 0.0;
    int valid_count = 0;

    for (size_t i = 0; i < source_points.size(); i++) {
        glm::vec3 p_local = glm::vec3(source_points[i].pos);

        // Current source point in world space
        glm::vec3 x = transform_point_world(source_point_cloud, p_local);

        // int target_id = find_closest_id(target_points_world, x, max_corr_dist_sq);
        // if (target_id < 0) {
        //     continue;
        // }

        int target_id = find_closest_id_with_valid_normal(
            target_points_world,
            target_normals_world,
            x,
            max_corr_dist_sq
        );
        if (target_id < 0) {
            // source_point_cloud.points[i].color = glm::vec4(1, 0, 0, 1);
            continue;
        }
        // source_point_cloud.points[i].color = glm::vec4(0, 0, 1, 1);
        

        glm::vec3 q = target_points_world[target_id];
        glm::vec3 raw_n = target_normals_world[target_id];

        float n_len2 = glm::dot(raw_n, raw_n);
        if (n_len2 < 1e-12f)
            continue;

        glm::vec3 n = raw_n / std::sqrt(n_len2);

        glm::vec3 a = glm::cross(x, n);
        double rhs = (double)glm::dot(n, q - x);

        total_sq_error += rhs * rhs;

        double J[6] = {
            (double)a.x,
            (double)a.y,
            (double)a.z,
            (double)n.x,
            (double)n.y,
            (double)n.z
        };

        for (int r = 0; r < 6; r++) {
            g[r] += J[r] * rhs;
            for (int c = 0; c < 6; c++) {
                H[r][c] += J[r] * J[c];
            }
        }

        valid_count++;
    }

    if (valid_count < 6) {
        std::cout << "Too few correspondences\n";
        return -1;
    }

    double rmse = std::sqrt(total_sq_error / (double)valid_count);

    // damping
    double lambda = 1e-6;
    for (int i = 0; i < 6; i++) {
        H[i][i] += lambda;
    }

    double delta[6] = {};
    bool ok = solve_6x6(H, g, delta);
    if (!ok) {
        std::cout << "solve_6x6 failed\n";
        return -1;
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

    // Current source cloud world rotation
    glm::mat3 R_src = euler_xyz_to_mat3(source_point_cloud.rotation);

    // ICP update in world space:
    // x' = dR * x + v
    glm::mat3 R_src_new = dR * R_src;
    glm::vec3 t_src_new = dR * source_point_cloud.position + v;

    source_point_cloud.position = t_src_new;
    source_point_cloud.rotation = mat3_to_euler_xyz(R_src_new);

    std::cout << "valid_count = " << valid_count
              << ", rmse = " << rmse
              << ", |omega| = " << glm::length(omega)
              << ", |v| = " << glm::length(v)
              << "\n";
    
    return rmse;
}

glm::mat3 skew_matrix(const glm::vec3& v) {
    glm::mat3 S(0.0f);

    // GLM matrices are column-major: m[col][row]
    S[0] = glm::vec3( 0.0f,  v.z,  -v.y);
    S[1] = glm::vec3(-v.z,   0.0f,  v.x);
    S[2] = glm::vec3( v.y,  -v.x,   0.0f);

    return S;
}

// Planar covariance from a normal:
// tangent variance = 1, normal variance = eps
glm::mat3 covariance_from_normal(const glm::vec3& raw_n, float eps = 1e-3f) {
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

void add_mat3_block(double H[6][6], int row0, int col0, const glm::mat3& M) {
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            H[row0 + r][col0 + c] += static_cast<double>(M[c][r]); // glm is [col][row]
        }
    }
}

void add_vec3_block(double g[6], int row0, const glm::vec3& v) {
    for (int r = 0; r < 3; r++) {
        g[row0 + r] += static_cast<double>(v[r]);
    }
}

glm::vec3 infrared_colormap(float t) {
    t = glm::clamp(t, 0.0f, 1.0f);

    float r = glm::clamp(1.5f - std::abs(4.0f * t - 3.0f), 0.0f, 1.0f);
    float g = glm::clamp(1.5f - std::abs(4.0f * t - 2.0f), 0.0f, 1.0f);
    float b = glm::clamp(1.5f - std::abs(4.0f * t - 1.0f), 0.0f, 1.0f);

    return glm::vec3(r, g, b);
}

double gicp_step(PointCloud& source_point_cloud,
                 const PointCloud& target_point_cloud,
                 const std::vector<glm::vec3>& source_normals,
                 const std::vector<glm::vec3>& target_normals)
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
    
    std::vector<float> source_distances(source_points.size(), -1);
    std::vector<float> target_indices(source_points.size(), -1);
    float max_new_dist = 0;
    // source_distances.reserve(source_points.size());

    for (size_t i = 0; i < target_points.size(); i++) {
        glm::vec3 q_local = glm::vec3(target_points[i].pos);
        glm::vec3 n_world = transform_normal_world(target_point_cloud, target_normals[i]);

        target_points_world.push_back(transform_point_world(target_point_cloud, q_local));
        target_normals_world.push_back(n_world);
        target_covs_world.push_back(covariance_from_normal(n_world, gicp_eps));
    }

    double total_weighted_sq_error = 0.0;
    int valid_count = 0;

    for (size_t i = 0; i < source_points.size(); i++) {
        glm::vec3 p_local = glm::vec3(source_points[i].pos);

        // Source point and source normal in world space
        glm::vec3 x = transform_point_world(source_point_cloud, p_local);
        glm::vec3 n_src_world = transform_normal_world(source_point_cloud, source_normals[i]);

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
            // source_point_cloud.points[i].color = glm::vec4(1, 0, 0, 1);
            continue;
        }
        source_distances[i] = glm::distance(target_points_world[target_id], x);
        if (source_distances[i] > max_new_dist)
            max_new_dist = source_distances[i];

        target_indices[i] = target_id;

        // source_point_cloud.points[i].color = glm::vec4(0, 1, 0, 1);
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
        // Since x and q are already in world space, both covariances are in world space too.
        glm::mat3 Sigma = C_B + C_A;
        glm::mat3 M = glm::inverse(Sigma);

        glm::vec3 d = q - x;

        // Linearization:
        // x' ≈ x + ω × x + v
        // d' = q - x' ≈ d + x×ω - v
        // so A = [skew(x), -I], residual = d
        glm::mat3 B = skew_matrix(x);
        glm::mat3 I(1.0f);

        glm::mat3 H00 = glm::transpose(B) * M * B;
        glm::mat3 H01 = -glm::transpose(B) * M;
        glm::mat3 H10 = -M * B;
        glm::mat3 H11 = M;

        glm::vec3 g0 = -glm::transpose(B) * M * d;
        glm::vec3 g1 =  M * d;

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

    // for (size_t i = 0; i < source_points.size(); i++) {
    //     if (target_indices[i] < 0 || source_distances[i] < 0)
    //         continue;
        
    //     glm::vec3 p_local = glm::vec3(source_points[i].pos);
    //     glm::vec3 x = transform_point_world(source_point_cloud, p_local);
    //     glm::vec3 q = target_points_world[target_indices[i]];

    //     float t = std::abs(glm::distance(x, q) - source_distances[i]) / max_new_dist;

    //     glm::vec3 color = infrared_colormap(t);
    //     source_point_cloud.points[i].color = glm::vec4(color, 1.0f);

    //     // if (new_dist < source_distances[i])
    //     //     source_point_cloud.points[i].color = glm::vec4(0, 1, 0, 1);
    //     // else
    //     //     source_point_cloud.points[i].color = glm::vec4(1, 0, 0, 1);
    // }

    
    

    std::cout << "valid_count = " << valid_count
              << ", weighted_rmse = " << rmse
              << ", |omega| = " << glm::length(omega)
              << ", |v| = " << glm::length(v)
              << "\n";

    return rmse;
}

void insert_scan_into_voxel_map(const PointCloud& scan_point_cloud,
                                const std::vector<glm::vec3>& scan_normals,
                                VoxelMap& voxel_map)
{
    const std::vector<PointInstance>& pts = scan_point_cloud.points;

    if (pts.size() != scan_normals.size()) {
        std::cout << "insert_scan_into_voxel_map: pts.size() != scan_normals.size()\n";
        return;
    }

    for (size_t i = 0; i < pts.size(); i++) {
        glm::vec3 p_local = glm::vec3(pts[i].pos);
        glm::vec3 p_world = transform_point_world(scan_point_cloud, p_local);

        glm::vec3 n_world = transform_normal_world(scan_point_cloud, scan_normals[i]);

        if (glm::dot(n_world, n_world) < 1e-12f)
            continue;

        voxel_map.insert_point(p_world, n_world);
    }
}

void rebuild_map_point_cloud_from_voxel_map(VoxelMap& voxel_map,
                                            PointCloud& map_point_cloud,
                                            std::vector<glm::vec3>& map_normals)
{
    std::vector<glm::vec3> map_points_world;
    voxel_map.extract_centroids_and_normals(map_points_world, map_normals);

    std::vector<PointInstance> instances;
    instances.reserve(map_points_world.size());

    for (size_t i = 0; i < map_points_world.size(); i++) {
        PointInstance p{};
        p.pos = glm::vec4(map_points_world[i], 1.0f);
        p.color = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f); // blue map
        instances.push_back(p);
    }

    map_point_cloud.points = instances;
    map_point_cloud.position = glm::vec3(0.0f);
    map_point_cloud.rotation = glm::vec3(0.0f);
    map_point_cloud.scale = glm::vec3(1.0f);

    map_point_cloud.update_points(map_point_cloud.points);
}

void initialize_map_from_first_scan(const PointCloud& first_scan_point_cloud,
                                    const std::vector<glm::vec3>& first_scan_normals,
                                    VoxelMap& voxel_map,
                                    PointCloud& map_point_cloud,
                                    std::vector<glm::vec3>& map_normals)
{
    voxel_map.clear();

    insert_scan_into_voxel_map(first_scan_point_cloud, first_scan_normals, voxel_map);
    rebuild_map_point_cloud_from_voxel_map(voxel_map, map_point_cloud, map_normals);
}

double align_scan_to_map_and_fuse(PointCloud& scan_point_cloud,
                                  const std::vector<glm::vec3>& scan_normals,
                                  PointCloud& map_point_cloud,
                                  const std::vector<glm::vec3>& map_normals,
                                  VoxelMap& voxel_map,
                                  int max_iterations = 10,
                                  double rmse_epsilon = 1e-4)
{
    if (map_point_cloud.points.empty()) {
        std::cout << "align_scan_to_map_and_fuse: map is empty\n";
        return -1.0;
    }

    double last_rmse = -1.0;
    double rmse = -1.0;

    for (int iter = 0; iter < max_iterations; iter++) {
        rmse = gicp_step(scan_point_cloud, map_point_cloud, scan_normals, map_normals);

        if (rmse < 0.0) {
            std::cout << "gicp_step failed\n";
            return -1.0;
        }

        if (last_rmse > 0.0 && std::abs(last_rmse - rmse) < rmse_epsilon) {
            break;
        }

        last_rmse = rmse;
    }

    // After the scan is aligned, fuse it into the voxel map
    insert_scan_into_voxel_map(scan_point_cloud, scan_normals, voxel_map);

    return rmse;
}

size_t remove_outlires(PointCloud& source_point_cloud,
                       std::vector<glm::vec3>& source_normals,
                       const PointCloud& target_point_cloud,
                       float max_distance)
{
    const std::vector<PointInstance>& source_points = source_point_cloud.points;
    const std::vector<PointInstance>& target_points = target_point_cloud.points;

    if (source_points.size() != source_normals.size()) {
        std::cout << "remove_outlires: source_points.size() != source_normals.size()\n";
        return 0;
    }

    if (target_points.empty()) {
        std::cout << "remove_outlires: target cloud is empty\n";
        return 0;
    }

    float max_dist_sq = max_distance * max_distance;

    // Precompute target points in world space
    std::vector<glm::vec3> target_points_world;
    target_points_world.reserve(target_points.size());

    for (size_t i = 0; i < target_points.size(); i++) {
        glm::vec3 q_local = glm::vec3(target_points[i].pos);
        target_points_world.push_back(transform_point_world(target_point_cloud, q_local));
    }

    std::vector<PointInstance> filtered_points;
    std::vector<glm::vec3> filtered_normals;

    filtered_points.reserve(source_points.size());
    filtered_normals.reserve(source_normals.size());

    size_t removed_count = 0;

    for (size_t i = 0; i < source_points.size(); i++) {
        glm::vec3 p_local = glm::vec3(source_points[i].pos);
        glm::vec3 x_world = transform_point_world(source_point_cloud, p_local);

        int target_id = find_closest_id(target_points_world, x_world, max_dist_sq);

        // No target point within max_distance -> remove as outlier
        if (target_id < 0) {
            removed_count++;
            continue;
        }

        filtered_points.push_back(source_points[i]);
        filtered_normals.push_back(source_normals[i]);
    }

    source_point_cloud.points = std::move(filtered_points);
    source_normals = std::move(filtered_normals);

    std::cout << "remove_outlires: removed " << removed_count
              << " points, kept " << source_point_cloud.points.size() << "\n";

    return removed_count;
}


int main() {
    Engine3D engine = Engine3D();
    Window window = Window(&engine, 1280, 720, "3D visualization");
    engine.set_window(&window);
    ui::init(window.window);

    Camera camera;
    window.set_camera(&camera);
    FPSCameraController camera_controller = FPSCameraController(&camera);
    
    camera_controller.speed = 50;

    VoxelGrid* voxel_grid = new VoxelGrid({16, 16, 16}, 1.0f, {24, 10, 24});
    voxel_grid->update(&window, &camera);
    sleep(1);

    glm::vec3 origin = glm::vec3(0.0f, 10.0f, 0.0f);

    std::vector<Line*> point_cloud_video_lines;
    std::vector<Point*> point_cloud_video_points;
    std::vector<Mesh*> lidar_meshes;

    PointCloudVideo point_cloud_video = PointCloudVideo();
    point_cloud_video.load_from_file("/home/spectre/TEMP_lidar_output_mesh/recording/index.csv");
    

    // // for (int i = 60; i < point_cloud_video.frames.size(); i++) {
    // for (int i = 201; i < 203; i++) {
    //     std::cout << "Iteration " << i << std::endl;
    //     double old_rmse = -1;
    //     // glm::vec4 old_position = point_cloud_video.frames[i-1].point_cloud.position;
    //     for (int j = 0; j < 10; j++) {
    //         // double rmse = icp_step(point_cloud_video.frames[i].point_cloud, point_cloud_video.frames[i-1].point_cloud, point_cloud_video.frames[i-1].normals);
    //         double rmse = gicp_step(point_cloud_video.frames[i].point_cloud, point_cloud_video.frames[200].point_cloud, point_cloud_video.frames[i].normals, point_cloud_video.frames[200].normals);

    //         if (rmse < 0.0)
    //             break;
    //         if (j > 0) {
    //             if (std::abs(old_rmse - rmse) < 1e-12f)
    //                 break;
    //         }

    //         old_rmse = rmse;
    //     }
    // }
    
    

    // PointCloudFrame point_cloud_frame = PointCloudFrame("/home/spectre/TEMP_lidar_output_mesh/recording/frame_000000.bin");
    // size_t rings_count = 16;
    // size_t ring_size = point_cloud_frame.point_cloud.size() / rings_count;
    
    // Mesh point_cloud_mesh = point_cloud_frame.point_cloud.generate_mesh_gpu(rings_count, ring_size, 1.5);


    std::vector<PointInstance> target_points;
    std::vector<glm::vec3> target_normals;

    std::vector<PointInstance> source_points;
    std::vector<PointInstance> source_points_for_drawing;

    // Small known transform used to generate source from target
    glm::vec3 source_rotation = glm::radians(glm::vec3(5.0f, -3.0f, 2.0f)); // pitch, yaw, roll-ish
    glm::vec3 source_translation = glm::vec3(12.0f, -4.0f, 7.0f);

    // Build rotation matrix once
    glm::mat4 R4 = glm::yawPitchRoll(
        source_rotation.y, // yaw   around Y
        source_rotation.x, // pitch around X
        source_rotation.z  // roll  around Z
    );
    glm::mat3 initial_R = glm::mat3(R4);

    size_t size_x = 50;
    size_t size_z = 50;
    std::mt19937 rng(1337);

    // Gaussian noise
    std::normal_distribution<float> tangent_noise_dist(0.0f, 0.18f); // lateral jitter
    std::normal_distribution<float> normal_noise_dist(0.0f, 0.05f);  // smaller normal jitter

    // Random dropout
    std::uniform_real_distribution<float> keep_dist(0.0f, 1.0f);
    float keep_probability = 0.88f; // keep ~88% of source points

    float amplitude = static_cast<float>(size_x) / 8.0f;

    for (int x = 0; x < size_x; x++) {
        for (int z = 0; z < size_z; z++) {
            PointInstance new_point{};

            new_point.pos.x = static_cast<float>(x);
            new_point.pos.z = static_cast<float>(z);
            new_point.pos.w = 1.0f;

            // Normalize grid coords to [-1, 1]
            float u = 2.0f * static_cast<float>(x) / static_cast<float>(size_x - 1) - 1.0f;
            float v = 2.0f * static_cast<float>(z) / static_cast<float>(size_z - 1) - 1.0f;

            // Two asymmetric Gaussian features
            float du1 = u - 0.35f;
            float dv1 = v + 0.20f;
            float e1 = std::exp(-(du1 * du1 + dv1 * dv1) / 0.08f);

            float du2 = u + 0.45f;
            float dv2 = v - 0.35f;
            float e2 = std::exp(-(du2 * du2 + dv2 * dv2) / 0.05f);

            // Non-repeating asymmetric height function
            float h =
                0.35f * u
                + 0.18f * v
                + 0.55f * u * u
                - 0.30f * v * v
                + 0.22f * u * v
                + 0.65f * e1
                - 0.40f * e2;

            new_point.pos.y = h * amplitude;
            new_point.color = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);

            target_points.push_back(new_point);

            // --- Analytic normal ---
            float du_dx = 2.0f / static_cast<float>(size_x - 1);
            float dv_dz = 2.0f / static_cast<float>(size_z - 1);

            float df_du =
                0.35f
                + 1.10f * u
                + 0.22f * v
                + 0.65f * e1 * (-2.0f * du1 / 0.08f)
                - 0.40f * e2 * (-2.0f * du2 / 0.05f);

            float df_dv =
                0.18f
                - 0.60f * v
                + 0.22f * u
                + 0.65f * e1 * (-2.0f * dv1 / 0.08f)
                - 0.40f * e2 * (-2.0f * dv2 / 0.05f);

            float dy_dx = amplitude * df_du * du_dx;
            float dy_dz = amplitude * df_dv * dv_dz;

            glm::vec3 normal(-dy_dx, 1.0f, -dy_dz);
            normal = glm::normalize(normal);

            target_normals.push_back(normal);

            // --------------------------
            // Build perturbed source point
            // --------------------------

            // Optional dropout: source scan sees only part of the surface samples
            if (keep_dist(rng) > keep_probability) {
                continue;
            }

            PointInstance src_point = new_point;
            glm::vec3 p3 = glm::vec3(new_point.pos);

            // First apply the rigid transform
            glm::vec3 transformed = initial_R * p3 + source_translation;

            // Build a tangent basis from the target normal
            glm::vec3 helper = (std::abs(normal.y) < 0.9f)
                ? glm::vec3(0.0f, 1.0f, 0.0f)
                : glm::vec3(1.0f, 0.0f, 0.0f);

            glm::vec3 tangent1 = glm::normalize(glm::cross(helper, normal));
            glm::vec3 tangent2 = glm::normalize(glm::cross(normal, tangent1));

            // LiDAR-ish perturbation:
            // mostly tangential jitter, smaller normal jitter
            float t1 = tangent_noise_dist(rng);
            float t2 = tangent_noise_dist(rng);
            float n_off = normal_noise_dist(rng);

            glm::vec3 local_offset =
                tangent1 * t1 +
                tangent2 * t2 +
                normal * n_off;

            transformed += local_offset;

            src_point.pos = glm::vec4(transformed, 1.0f);
            src_point.color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

            source_points.push_back(src_point);
            source_points_for_drawing.push_back(src_point);
        }
    }
    
    PointCloud target_point_cloud;
    PointCloud source_point_cloud;
    PointCloud source_point_cloud_for_drawing;
    target_point_cloud.update_points(target_points);
    source_point_cloud.update_points(source_points);
    

    glm::mat3 R = glm::mat3(1.0f);
    glm::vec3 t = glm::vec3(0.0f);
    
    
    source_point_cloud_for_drawing.update_points(source_points_for_drawing);



    CirlceInstance circle_instance;
    // circle_instance.color = glm::vec4(1, 1, 1, 1);
    circle_instance.color = glm::vec4(0.8, 0.8, 0.2, 1);

    std::vector<CirlceInstance> instances;

    int row_size = 8;
    for (int x = 0; x < row_size; x++)
        for (int z = 0; z < row_size; z++) {
            circle_instance.position_radius.x = (x + 0.5) * 2;
            circle_instance.position_radius.z = (z + 0.5) * 2;

            circle_instance.color = glm::vec4((float)x / (float)row_size, 0, (float)z / (float)row_size, 1);

            instances.push_back(circle_instance);
        }
    
    CircleCloud circle_cloud = CircleCloud();
    circle_cloud.set_instances(instances);

    LightSource light_source;
    light_source.position = glm::vec4(-15, 2, 2, 8);
    light_source.color = glm::vec4(1, 0, 0, 1);
    engine.lighting_system.set_light_source(0, light_source);

    light_source.position = glm::vec4(-15, 2, -2, 8);
    light_source.color = glm::vec4(0, 1, 0, 1);
    engine.lighting_system.set_light_source(1, light_source);

    light_source.position = glm::vec4(-13, 2, 0, 8);
    light_source.color = glm::vec4(0, 0, 1, 1);
    engine.lighting_system.set_light_source(2, light_source);

    light_source.position = glm::vec4(-17, 2, 0, 8);
    light_source.color = glm::vec4(1, 1, 1, 1);
    engine.lighting_system.set_light_source(3, light_source);

    Sphere sphere = Sphere();
    sphere.mesh->position.x = -14;
    sphere.mesh->position.y = 1;

    PBRSkybox skybox = PBRSkybox(engine.texture_manager->st_peters_square_night_4k);

    

    int id_1 = 200;
    int id_2 = 221;

    for (int i = 0; i < point_cloud_video.frames[id_1].point_cloud.points.size(); i++) {
        point_cloud_video.frames[id_1].point_cloud.points[i].color = glm::vec4(0, 0, 1, 1);
    }
    point_cloud_video.frames[id_1].point_cloud.update_points(point_cloud_video.frames[id_1].point_cloud.points);

    // for (int i = 0; i < point_cloud_video.frames[id_2].point_cloud.points.size(); i++) {
    //     point_cloud_video.frames[id_2].point_cloud.points[i].color = glm::vec4(1, 0, 0, 1);
    // }
    // point_cloud_video.frames[id_2].point_cloud.update_points(point_cloud_video.frames[id_2].point_cloud.points);
    VoxelMap voxel_map(0.5f); // choose voxel size
    PointCloud map_point_cloud;
    std::vector<glm::vec3> map_normals;
    // initialize_map_from_first_scan(point_cloud_video.frames[0].point_cloud,
    //                                 point_cloud_video.frames[0].normals,
    //                                 voxel_map,
    //                                 map_point_cloud,
    //                                 map_normals);
    
    // for (int i = 1; i < point_cloud_video.frames.size(); i++) {
    //     PointCloud& scan_cloud = point_cloud_video.frames[i].point_cloud;
    //     std::vector<glm::vec3>& scan_normals = point_cloud_video.frames[i].normals;

    //     double rmse = align_scan_to_map_and_fuse(scan_cloud,
    //                                             scan_normals,
    //                                             map_point_cloud,
    //                                             map_normals,
    //                                             voxel_map,
    //                                             10,
    //                                             1e-4);

    //     if (rmse < 0.0) {
    //         std::cout << "Failed to align frame " << i << "\n";
    //         continue;
    //     }

    //     rebuild_map_point_cloud_from_voxel_map(voxel_map, map_point_cloud, map_normals);

    //     std::cout << "Frame " << i << " fused into map, rmse = " << rmse << "\n";
    // }
    
    



    remove_outlires(point_cloud_video.frames[id_2].point_cloud, point_cloud_video.frames[id_2].normals, point_cloud_video.frames[id_1].point_cloud, 5.0f);
    point_cloud_video.frames[id_2].point_cloud.update_points(point_cloud_video.frames[id_2].point_cloud.points);
        
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

        // window.draw(&skybox, &camera);

        skybox.attach_environment(*engine.default_program);
        skybox.attach_environment(*engine.default_circle_program);

        // sphere.mesh->position.x = -14 + cos(timer) * 4;
        // sphere.mesh->position.z = 0 + sin(timer) * 4;

        // voxel_grid->update(&window, &camera);
        // window.draw(voxel_grid, &camera);

        // window.draw(&point_cloud_mesh, &camera);

        // window.draw(&target_point_cloud, &camera);
        // window.draw(&source_point_cloud, &camera);

        // point_cloud_video.update(delta_time);
        // window.draw(&point_cloud_video.frames[id_1], &camera);
        window.draw(&map_point_cloud, &camera);
        
        // window.draw(&point_cloud_video.frames[id_2], &camera);
        window.draw(&point_cloud_video.frames[cur_frame], &camera);
        // window.draw(&point_cloud_video.frames[61], &camera);

        


        // window.draw(&source_point_cloud_for_drawing, &camera);
        
        
        
        
        // window.draw(&circle_cloud, &camera);
        // window.draw(sphere.mesh, &camera);

        ui::begin_frame();
        ui::update_mouse_mode(&window);

        ImGui::Begin("Debug");

        glm::vec3 p = camera.position; // or any vec3

        ImGui::TextColored(ImVec4(1,0.5,0.5,1), "x: %.3f", p.x);
        ImGui::TextColored(ImVec4(0.5,1,0.5,1), "y: %.3f", p.y);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "z: %.3f", p.z);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "fps: %.3f", fps);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "current frame: %d", (int)point_cloud_video.current_frame);
        ImGui::InputInt("current frame", &cur_frame);
        

        ImGui::SliderFloat("Relative threshold", &rel_thresh, 0.0f, 10.0f);

        if (ImGui::Button("ICP Step")) {
            // icp_step(source_points, target_points, target_normals, R, t);
            // icp_step(source_point_cloud, target_point_cloud, target_normals);
            gicp_step(point_cloud_video.frames[cur_frame].point_cloud, point_cloud_video.frames[id_1].point_cloud, point_cloud_video.frames[cur_frame].normals, point_cloud_video.frames[id_1].normals);
            point_cloud_video.frames[cur_frame].point_cloud.update_points(point_cloud_video.frames[cur_frame].point_cloud.points);
            // icp_step(point_cloud_video.frames[id_2].point_cloud, point_cloud_video.frames[id_1].point_cloud, point_cloud_video.frames[id_1].normals);
            

            // for (size_t i = 0; i < source_points.size(); i++) {
            //     glm::vec3 p = R * glm::vec3(source_points[i].pos) + t;
            //     source_points[i].pos = glm::vec4(p, 1.0f);
            // }

            // R = glm::mat3(1.0f);
            // t = glm::vec3(0.0f);

            // source_point_cloud.update_points(source_points);
        }

        if (ImGui::Button("Initialize Map From Current Frame")) {
            initialize_map_from_first_scan(point_cloud_video.frames[cur_frame].point_cloud,
                                        point_cloud_video.frames[cur_frame].normals,
                                        voxel_map,
                                        map_point_cloud,
                                        map_normals);
            map_initialized = true;
        }

        if (ImGui::Button("Align Current Frame To Map And Fuse")) {
            if (!map_initialized) {
                std::cout << "Map is not initialized\n";
            } else {
                double rmse = align_scan_to_map_and_fuse(point_cloud_video.frames[cur_frame].point_cloud,
                                                        point_cloud_video.frames[cur_frame].normals,
                                                        map_point_cloud,
                                                        map_normals,
                                                        voxel_map,
                                                        10,
                                                        1e-4);

                if (rmse >= 0.0) {
                    rebuild_map_point_cloud_from_voxel_map(voxel_map, map_point_cloud, map_normals);
                    std::cout << "Fused frame " << cur_frame << ", rmse = " << rmse << "\n";
                }
            }
        }

        ImGui::End();

        ui::end_frame();
        

        window.swap_buffers();
        engine.poll_events();
    }
    
    ui::shutdown();
}
