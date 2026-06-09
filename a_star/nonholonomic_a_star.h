#pragma once
// #include "a_star.h"
#include "curvature_a_star.h"
#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include <cmath>
#include "../line.h"
#include "../nonholonomic_pos.h"
#include "reeds_shepp.h"
#include <algorithm>
#include <chrono>


// struct NonholonomicPos {
//     glm::vec3 pos = glm::vec3(0.0f, 0.0f, 0.0f);
//     float theta = 0; // orientation

//     float steer = 0;
//     float dir = 1;
//     // std::vector<NonholonomicPos> motion;

//     // bool operator==(const NonholonomicPos& other) const {
//     //     static constexpr float pos_eps = 1e-3f;
//     //     static constexpr float theta_eps = 1e-3f;

//     //     bool p = glm::all(glm::epsilonEqual(pos, other.pos, pos_eps));
//     //     bool t = std::abs(theta - other.theta) <= theta_eps;
//     //     return p && t;
//     // }
// };


struct AvgTimer {
    std::chrono::steady_clock::duration total{};
    std::size_t n = 0;
    std::chrono::_V2::steady_clock::time_point start_point;

    void start() {
        start_point = std::chrono::steady_clock::now();
    }

    void end() {
        auto end_point = std::chrono::steady_clock::now();

        add(end_point - start_point);

        start_point = std::chrono::_V2::steady_clock::time_point();
    }

    void add(std::chrono::steady_clock::duration d) { total += d; ++n; }

    double average_ms() const {
        return n
            ? std::chrono::duration<double, std::milli>(total).count() / n
            : 0.0;
    }
};

struct NonholonomicAStarCell {
    float g;
    float f;
    NonholonomicPos pos;
    NonholonomicPos came_from;
    bool no_parent = true;

    
};

class NonholonomicKeyPacker {
public:
    // --- discretization ---
    static constexpr float POS_RES = 0.5f;   // world units per grid cell (tweak)
    static constexpr int   THETA_BINS = 128;  // tweak (16/32/64...)

    // --- bit layout ---
    static constexpr int BITS_POS  = 18;     // per axis
    static constexpr int BITS_TH   = 8;      // enough for 0..255 bins (THETA_BINS must <= 256)

    static_assert(THETA_BINS <= (1 << BITS_TH));
    static_assert(3*BITS_POS + BITS_TH <= 64);

    static constexpr uint64_t MASK_POS = (BITS_POS == 64) ? ~0ull : ((1ull << BITS_POS) - 1ull);
    static constexpr uint64_t MASK_TH  = (BITS_TH  == 64) ? ~0ull : ((1ull << BITS_TH)  - 1ull);

    static constexpr int32_t OFFSET_POS = 1 << (BITS_POS - 1); // for signed -> unsigned bias
    static constexpr int32_t MIN_POS = -OFFSET_POS;
    static constexpr int32_t MAX_POS =  OFFSET_POS - 1;

    static inline uint64_t enc_pos(int32_t c) {
        if (c < MIN_POS || c > MAX_POS)
            throw std::out_of_range("pos coord out of packable range");
        return (uint64_t)((int64_t)c + OFFSET_POS) & MASK_POS;
    }

    static inline uint64_t enc_th(int32_t t) {
        if (t < 0 || t >= THETA_BINS)
            throw std::out_of_range("theta bin out of range");
        return (uint64_t)t & MASK_TH;
    }

    static inline int32_t qfloor(float v, float step) {
        return (int32_t)std::floor(v / step);
    }

    static inline float wrap_2pi(float a) {
        const float TWO_PI = 6.2831853071795864769f;
        a = std::fmod(a, TWO_PI);
        if (a < 0.0f) a += TWO_PI;
        return a;
    }

    static uint64_t pack(const NonholonomicPos& s) {
        int32_t ix = qfloor(s.pos.x, POS_RES);
        int32_t iy = qfloor(s.pos.y, POS_RES);
        int32_t iz = qfloor(s.pos.z, POS_RES);

        float a = wrap_2pi(s.theta);
        int32_t it = (int32_t)std::floor(a * (float)THETA_BINS / 6.2831853071795864769f);
        if (it == THETA_BINS) it = 0; // just in case of a==2pi

        uint64_t ux = enc_pos(ix);
        uint64_t uy = enc_pos(iy);
        uint64_t uz = enc_pos(iz);
        uint64_t ut = enc_th(it);

        // Layout: [x][y][z][theta]
        return (ux << (BITS_POS*2 + BITS_TH))
             | (uy << (BITS_POS*1 + BITS_TH))
             | (uz << (BITS_TH))
             | (ut);
    }
};

struct NonholonomicByPriority {
    bool operator()(const NonholonomicAStarCell& a, const NonholonomicAStarCell& b) const {
        return a.f > b.f; // higher priority first
    }
};

struct DistToPathData {
    float dist;
    float id;
};

// class NonholonomicAStar : public AStar {
class NonholonomicAStar : public CurvatureAStar {
public:
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
    uint64_t state_counter = 0;
    int iteration_limit = 10000;
    bool track_explored_paths = true;
    bool TEMPPPPPTESTTTT = false;

    AvgTimer motion_simulation_time;
    AvgTimer adjust_to_ground_time;
    AvgTimer get_ground_positions_time;
    AvgTimer crosses_extreme_curvature_time;
    AvgTimer get_nonholonomic_f_time;

    NonholonomicAStar(VoxelGrid* voxel_grid);

    static inline float angle_diff(float a, float b) {
        // result in (-pi, pi]
        float d = std::fmod(b - a, 2.0f * (float)M_PI);
        if (d <= -(float)M_PI) d += 2.0f * (float)M_PI;
        if (d >  (float)M_PI) d -= 2.0f * (float)M_PI;
        return d;
    }

    uint64_t state_key(const NonholonomicPos& s) const {
        // pick a resolution that matches your planner step / grid
        constexpr float POS_RES = 0.5f;

        int32_t ix = (int32_t)std::floor(s.pos.x / POS_RES);
        int32_t iz = (int32_t)std::floor(s.pos.z / POS_RES);
        int32_t it = discretize_angle(s.theta, num_theta_bins);

        return grid->pack_key(ix, iz, it);
    }

    
    static void print_vec(glm::vec3 vec) {
        std::cout << "(" << vec.x << ", " << vec.y << ", " << vec.z << ")";
    }

    DistToPathData dist_to_path(glm::ivec3 pos, std::vector<glm::ivec3>& path);
    DistToPathData max_unimpended_dist_to_path(glm::vec3 pos, std::vector<glm::ivec3>& path, int start_id = 0, glm::vec3 last_pos = glm::vec3(0, 0, 0), bool replace_last_pos = false);
    DistToPathData dist_to_path_dubins(NonholonomicPos pos, std::vector<glm::ivec3>& path);
    float follow_plain_astar_heuristic(glm::ivec3 pos, std::vector<glm::ivec3>& path, float scale = 1.0f, float dist_to_path_threshold = 2.0f);
    static int discretize_angle(float value, int num_bins);
    float get_nonholonomic_f(NonholonomicPos& new_pos, NonholonomicPos end_pos, NonholonomicPos cur_pos, PlainAstarData plain_a_star_path);
    std::vector<NonholonomicPos> find_reeds_shepp(NonholonomicPos start_pos, NonholonomicPos end_pos);
    double reeds_shepp_distance(NonholonomicPos& start_pos, NonholonomicPos& end_pos);
    bool shot_reeds_shepp(NonholonomicPos start_pos, NonholonomicPos end_pos);
    bool adjust_and_check_path(std::vector<NonholonomicPos>& path, int max_step_up = 1, int max_drop = 1);
    // bool adjust_to_ground(glm::ivec3& voxel_pos, int max_step_up = 1, int max_drop = 1);
    static bool almost_equal(NonholonomicPos a, NonholonomicPos b);
    std::vector<NonholonomicPos> reconstruct_path(std::unordered_map<uint64_t, NonholonomicAStarCell> closed_heap, NonholonomicPos pos);
    std::vector<NonholonomicPos> simulate_motion(NonholonomicPos start_pos, int steer, int direction);
    void initialize(NonholonomicPos start_pos, NonholonomicPos end_pos);
    std::vector<glm::ivec3> get_ground_cells(glm::vec3 p0, glm::vec3 p1);
    bool crosses_extreme_curvature(const std::vector<NonholonomicPos>& path, float curvature_limit);
    bool crosses_extreme_curvature(const std::vector<glm::ivec3>& path, float curvature_limit);
    bool try_reeds_shepp_shot(NonholonomicPos& start, NonholonomicPos& end, std::vector<NonholonomicPos>& out_path);
    bool try_finish_with_reeds_shepp(NonholonomicPos& from, NonholonomicPos& to);
    bool find_nonholomic_path_step();
    void find_nonholomic_path();
    // std::vector<NonholonomicPos> find_nonholomic_path(NonholonomicPos start_pos, NonholonomicPos end_pos);
    
};