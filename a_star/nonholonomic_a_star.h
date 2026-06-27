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
#include "../utils/avg_timer.h"
#include "a_star_structures.h"
#include "../math_utils.h"
#include "a_star.h"

#include "../vulkan_self/logger/logger_header.h"

class VoxelGrid;
class VulkanSubmitContext;
class UnimpendedPathFinder;

class NonholonomicAStar {
public:
    _XCLASS_NAME(NonholonomicAStar);
    
    struct NonholonomicAStarParams {
        float wheel_base = 2.5f;
        float max_steer = 0.6;
        float min_radius = 0.0f;
        float integration_steps = 8;
        float motion_simulation_dist = 1.5f;
        float reeds_shepp_step_world = 0.10f;
        int try_reeds_shepp_interval = 100;
        int num_theta_bins = 128;
        bool use_reed_shepps_fallback = false;
        bool force_reeds_shepp_shot = false;
        bool allow_flying_over_precipices = true;
        bool allow_diagonal_moves = false;
        int iteration_limit = 20000;
        bool track_explored_paths = true;
        int max_step_up = 1;
        int max_drop = 1;
        int max_y_diff = 1;
        float max_goal_position_error = 0.7f;
        float max_goal_heading_error_radians = 0.3f;
    };

    struct NonholonomicAStarState {
        std::priority_queue<
            NonholonomicAStarCell, 
            std::vector<NonholonomicAStarCell>, 
            NonholonomicByPriority
        > pq;
        std::unordered_map<uint64_t, NonholonomicAStarCell> closed_heap;
        std::unordered_map<uint64_t, float> g_score;
        NonholonomicPos start_pos;
        NonholonomicPos end_pos;
        std::vector<NonholonomicPos> path;
        std::vector<LineInstance> explored_paths;
        PlainAstarData plain_astar_path;
        std::vector<NonholonomicPos> unimpended_astar_positions;
        std::vector<float> dubins_segment_lengths;
        std::vector<float> dubins_distance_to_end;
        uint64_t counter = 0;
    };

    typedef NonholonomicAStarParams NonholonomicAStarDesc;

    NonholonomicAStar(
        OccupancyGrid3D& occupancy_grid,
        const NonholonomicAStarDesc& desc,
        UnimpendedPathFinder& unimpened_path_finder
    );

    uint64_t state_key(const NonholonomicPos& s) const;
    static float angle_diff(float a, float b);
    // DistToPathData max_unimpended_dist_to_path(glm::vec3 pos, std::vector<glm::ivec3>& path, int start_id = 0, glm::vec3 last_pos = glm::vec3(0, 0, 0), bool replace_last_pos = false);
    static int discretize_angle(float value, int num_bins);
    float get_nonholonomic_f(NonholonomicPos& new_pos, NonholonomicPos end_pos, NonholonomicPos cur_pos);
    static bool almost_equal(
        NonholonomicPos a, 
        NonholonomicPos b, 
        float max_goal_position_error, 
        float max_goal_heading_error_radians,
        bool allow_flying_over_precipices);
    std::vector<NonholonomicPos> reconstruct_path(std::unordered_map<uint64_t, NonholonomicAStarCell> closed_heap, NonholonomicPos pos);
    std::vector<NonholonomicPos> simulate_motion(NonholonomicPos start_pos, int steer, int direction);
    bool try_reeds_shepp_shot(NonholonomicPos& start, NonholonomicPos& end, std::vector<NonholonomicPos>& out_path);
    bool try_finish_with_reeds_shepp(NonholonomicPos& from, NonholonomicPos& to);

    std::vector<glm::ivec3> find_unimpended_points(VulkanSubmitContext& submit_context, const PlainAstarData& astar_path_data);
    std::vector<NonholonomicPos> prepare_unimpended_points(
        const std::vector<glm::ivec3>& unimpended_points,
        NonholonomicPos start_pos,
        NonholonomicPos end_pos
    );
    
    void initialize(VulkanSubmitContext& submit_context, NonholonomicPos start_pos, NonholonomicPos end_pos);
    bool find_nonholomic_path_step();
    void find_nonholomic_path();
    OccupancyGrid3D& occupancy_grid() noexcept;
    NonholonomicAStarState& state() noexcept;

    DistToPathData max_unimpended_dist_to_path(glm::vec3 pos, std::vector<glm::ivec3>& path, int start_id, glm::vec3 last_pos, bool replace_last_pos);

private:
    NonholonomicAStarParams m_params;
    NonholonomicAStarState m_state;
    OccupancyGrid3D* m_grid = nullptr;
    UnimpendedPathFinder* m_unimpened_path_finder = nullptr;
    AStar m_plain_astar;
};
