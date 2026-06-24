#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include <cmath>
#include <queue>

#include <algorithm>
#include <chrono>
#include <numbers>
#include <chrono>
#include <cstddef>
#include <functional>
#include <vector>
#include <cstdint>

#include "../renderer/lines/line_instance.h"
#include "a_star_structures.h"
#include "../math_utils.h"
#include "a_star.h"

struct AvgTimer {
    std::chrono::steady_clock::duration total{};
    std::size_t n = 0;
    std::chrono::steady_clock::time_point start_point{};

    void start() {
        start_point = std::chrono::steady_clock::now();
    }

    void end() {
        auto end_point = std::chrono::steady_clock::now();
        add(end_point - start_point);
        start_point = std::chrono::steady_clock::time_point{};
    }

    void add(std::chrono::steady_clock::duration d) {
        total += d;
        ++n;
    }

    double average_ms() const {
        return n
            ? std::chrono::duration<double, std::milli>(total).count() / n
            : 0.0;
    }
};

class VoxelGrid;

// class NonholonomicAStar : public AStar {
class NonholonomicAStar : public AStar {
public:
    using UnimpendedPathProvider = std::function<std::vector<glm::vec3>(
        const PlainAstarData& astar_path_data,
        uint32_t max_step_up,
        uint32_t max_drop
    )>;

    NonholonomicAStar(VoxelGrid& voxel_grid, UnimpendedPathProvider unimpended_path_provider);

    std::priority_queue<NonholonomicAStarCell, std::vector<NonholonomicAStarCell>, NonholonomicByPriority> state_pq;
    std::unordered_map<uint64_t, NonholonomicAStarCell> state_closed_heap;
    std::unordered_map<uint64_t, float> state_g_score;
    NonholonomicPos state_start_pos;
    NonholonomicPos state_end_pos;
    NonholonomicAStarCell state_start_cell;
    std::vector<NonholonomicPos> state_path;
    // std::vector<Line*> state_lines;
    std::vector<LineInstance> state_explored_paths;
    PlainAstarData state_plain_astar_path;
    float state_plain_astar_path_length = 999999;
    std::vector<NonholonomicPos> unimpended_astar_positions;
    std::vector<float> dubins_segment_lengths;
    std::vector<float> dubins_distance_to_end;

    float wheel_base = 2.5f;
    float max_steer = 0.6;
    // float wheel_base = 1.0f;
    float min_radius = 0.0f;
    float integration_steps = 8;
    // float motion_simulation_dist = 0.2f;
    float motion_simulation_dist = 1.5f;
    float reeds_shepp_step_world = 0.10f;
    int try_reeds_shepp_interval = 100;
    // int num_theta_bins = 32;
    int num_theta_bins = 128;
    float switch_dir_pentalty = 1.5;
    float change_steer_pentalty = 1.5;
    bool use_reed_shepps_fallback = false;
    bool force_reeds_shepp_shot = false;
    bool allow_flying_over_precipices = true;
    uint64_t state_counter = 0;
    int iteration_limit = 10000;
    bool track_explored_paths = true;
    bool TEMPPPPPTESTTTT = false;

    AvgTimer motion_simulation_time;
    AvgTimer adjust_to_ground_time;
    AvgTimer get_ground_positions_time;
    AvgTimer crosses_extreme_curvature_time;
    AvgTimer get_nonholonomic_f_time;

    // NonholonomicAStar(VoxelGrid* voxel_grid);

    static inline float angle_diff(float a, float b) {
        constexpr float pi = std::numbers::pi_v<float>;
        float d = std::fmod(b - a, 2.0f * pi);
        if (d <= -pi) d += 2.0f * pi;
        if (d >   pi) d -= 2.0f * pi;
        return d;
    }

    uint64_t state_key(const NonholonomicPos& s) const {
        // pick a resolution that matches your planner step / grid
        constexpr float POS_RES = 0.5f;

        int32_t ix = (int32_t)std::floor(s.pos.x / POS_RES);
        int32_t iz = (int32_t)std::floor(s.pos.z / POS_RES);
        int32_t it = discretize_angle(s.theta, num_theta_bins);

        return math_utils::pack_key(ix, iz, it);
    }
    
    // static void print_vec(glm::vec3 vec) {
    //     std::cout << "(" << vec.x << ", " << vec.y << ", " << vec.z << ")";
    // }

    DistToPathData dist_to_path(glm::ivec3 pos, std::vector<glm::ivec3>& path);
    // DistToPathData max_unimpended_dist_to_path(glm::vec3 pos, std::vector<glm::mivec3>& path, int start_id = 0, glm::vec3 last_pos = glm::vec3(0, 0, 0), bool replace_last_pos = false);
    DistToPathData dist_to_path_dubins(NonholonomicPos pos, std::vector<glm::ivec3>& path);
    float follow_plain_astar_heuristic(glm::ivec3 pos, std::vector<glm::ivec3>& path, float scale = 1.0f, float dist_to_path_threshold = 2.0f);
    static int discretize_angle(float value, int num_bins);
    float get_nonholonomic_f(NonholonomicPos& new_pos, NonholonomicPos end_pos, NonholonomicPos cur_pos, PlainAstarData plain_a_star_path);
    std::vector<NonholonomicPos> find_reeds_shepp(NonholonomicPos start_pos, NonholonomicPos end_pos);
    double reeds_shepp_distance(NonholonomicPos& start_pos, NonholonomicPos& end_pos);
    bool shot_reeds_shepp(NonholonomicPos start_pos, NonholonomicPos end_pos);
    bool adjust_and_check_path(std::vector<NonholonomicPos>& path, int max_step_up = 1, int max_drop = 1);
    // bool adjust_to_ground(glm::ivec3& voxel_pos, int max_step_up = 1, int max_drop = 1);
    static bool almost_equal(NonholonomicPos a, NonholonomicPos b, bool allow_flying_over_precipices);
    std::vector<NonholonomicPos> reconstruct_path(std::unordered_map<uint64_t, NonholonomicAStarCell> closed_heap, NonholonomicPos pos);
    std::vector<NonholonomicPos> simulate_motion(NonholonomicPos start_pos, int steer, int direction);


    std::vector<glm::vec3> find_unimpended_path(
        const PlainAstarData& astar_path_data
    );
    void initialize(NonholonomicPos start_pos, NonholonomicPos end_pos);
    std::vector<glm::ivec3> get_ground_cells(glm::vec3 p0, glm::vec3 p1);
    bool crosses_extreme_curvature(const std::vector<NonholonomicPos>& path, float curvature_limit);
    bool crosses_extreme_curvature(const std::vector<glm::ivec3>& path, float curvature_limit);
    bool try_reeds_shepp_shot(NonholonomicPos& start, NonholonomicPos& end, std::vector<NonholonomicPos>& out_path);
    bool try_finish_with_reeds_shepp(NonholonomicPos& from, NonholonomicPos& to);
    bool find_nonholomic_path_step();
    void find_nonholomic_path();
    // // std::vector<NonholonomicPos> find_nonholomic_path(NonholonomicPos start_pos, NonholonomicPos end_pos);

private:
    UnimpendedPathProvider m_unimpended_path_provider;
};
