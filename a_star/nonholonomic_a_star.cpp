#include "nonholonomic_a_star.h"

#include "../vulkan_self/vulkan_submit_context.h"
#include "unimpended_path_finder.h"
#include "reeds_shepp.h"

#include <iostream>

namespace {
    void insert_y_transition_points(std::vector<NonholonomicPos>& path) {
        constexpr float eps = 1e-6f;

        if (path.size() < 2) {
            return;
        }

        std::vector<NonholonomicPos> corrected_path;
        corrected_path.reserve(path.size() * 2);
        corrected_path.push_back(path.front());

        for (size_t i = 1; i < path.size(); i++) {
            const NonholonomicPos& previous = path[i - 1];
            const NonholonomicPos& current = path[i];

            float y_delta = current.pos.y - previous.pos.y;
            if (std::abs(y_delta) > eps) {
                NonholonomicPos intermediate_point;

                if (y_delta > 0.0f) {
                    intermediate_point = previous;
                    intermediate_point.pos.y = current.pos.y;
                } else {
                    intermediate_point = current;
                    intermediate_point.pos.y = previous.pos.y;
                }

                corrected_path.push_back(intermediate_point);
            }

            corrected_path.push_back(current);
        }

        path = std::move(corrected_path);
    }
}

NonholonomicAStar::NonholonomicAStar(
    OccupancyGrid3D& occupancy_grid, 
    const NonholonomicAStarDesc& desc,
    UnimpendedPathFinder& unimpened_path_finder)
    :   m_params(desc),
        m_grid(&occupancy_grid),
        m_unimpened_path_finder(&unimpened_path_finder),
        m_plain_astar(occupancy_grid, AStar::AStarDesc{
            .max_step_up = desc.max_step_up,
            .max_drop = desc.max_drop,
            .max_y_diff = desc.max_y_diff,
            .iteration_limit = desc.iteration_limit,
            .allow_diagonal_moves = desc.allow_diagonal_moves,
            .allow_flying_over_precepices = desc.allow_flying_over_precipices
        }) {}

std::vector<NonholonomicPos> NonholonomicAStar::simulate_motion(NonholonomicPos start, int steer, int direction)
{
    LOG_METHOD();

    steer = std::clamp(steer, -1, 1);
    direction = (direction < 0) ? -1 : 1; // only -1 or +1

    float delta = 0.0f;
    if (steer != 0) delta = -steer * m_params.max_steer;

    std::vector<NonholonomicPos> out;
    out.reserve(m_params.integration_steps);

    glm::vec3 p = start.pos;
    float yaw   = start.theta;

    const float ds = m_params.motion_simulation_dist / float(m_params.integration_steps);
    const float ds_signed = ds * float(direction);
    const float eps = 1e-6f;

    for (int i = 0; i < m_params.integration_steps; ++i) {
        if (std::abs(delta) < eps) {
            // straight (forward or reverse)
            p.x += ds_signed * std::cos(yaw);
            p.z += ds_signed * std::sin(yaw);
        } else {
            // exact circular arc integration (forward or reverse)
            float tanD = std::tan(delta);
            // (tanD won't be ~0 here because of the eps check above)
            float R = m_params.wheel_base / tanD;   // signed radius
            float yaw0 = yaw;
            float dYaw = ds_signed / R;    // signed
            yaw += dYaw;

            p.x += R * (std::sin(yaw) - std::sin(yaw0));
            p.z += R * (-std::cos(yaw) + std::cos(yaw0));
        }

        NonholonomicPos s = start;
        s.pos = p;
        s.theta = yaw;
        out.push_back(s);
    }

    return out;
}

bool NonholonomicAStar::almost_equal(
        NonholonomicPos a, 
        NonholonomicPos b, 
        float max_goal_position_error, 
        float max_goal_heading_error_radians, 
        bool allow_flying_over_precipices) {        
    LOG_NAMED("NonholonomicAStar");

    if (allow_flying_over_precipices) {
        a.pos.y = 0;
        b.pos.y = 0;
    }    
    
    float dist = glm::distance(a.pos, b.pos);
    float theta_diff = std::abs(angle_diff(a.theta, b.theta));
    
    bool almost_equal_pos = dist <= max_goal_position_error;
    bool almost_equal_angle = theta_diff <= max_goal_heading_error_radians;

    return almost_equal_pos && almost_equal_angle;
}

std::vector<NonholonomicPos> NonholonomicAStar::reconstruct_path(std::unordered_map<uint64_t, NonholonomicAStarCell> closed_heap, NonholonomicPos pos) {
    LOG_METHOD();

    std::vector<NonholonomicPos> path;
    NonholonomicPos cur_pos = pos;

    while (true) {
        uint64_t cur_key = state_key(cur_pos);

        auto it = closed_heap.find(cur_key);

        if (it == closed_heap.end())
            return {};
            
        NonholonomicAStarCell cur_cell = it->second;

        path.push_back(cur_pos);

        if (cur_cell.no_parent)
            break;
        
        float eps = 1e-6f;
        float y_delta = cur_pos.pos.y - cur_cell.came_from.pos.y;
        if (std::abs(y_delta) > eps) {
            NonholonomicPos intermediate_point;

            if (y_delta > 0.0f) {
                intermediate_point = cur_cell.came_from;
                intermediate_point.pos.y = cur_pos.pos.y;
            } else {
                intermediate_point = cur_pos;
                intermediate_point.pos.y = cur_cell.came_from.pos.y;
            }

            path.push_back(intermediate_point);
        }
        
        cur_pos = cur_cell.came_from;
    }

    std::reverse(path.begin(), path.end());

    return path;
}

uint64_t NonholonomicAStar::state_key(const NonholonomicPos& s) const {
    LOG_METHOD();

    constexpr float POS_RES = 0.5f;

    int32_t ix = (int32_t)std::floor(s.pos.x / POS_RES);
    int32_t iz = (int32_t)std::floor(s.pos.z / POS_RES);
    int32_t it = discretize_angle(s.theta, m_params.num_theta_bins);

    return math_utils::pack_key(ix, iz, it);
}

float NonholonomicAStar::angle_diff(float a, float b) {
    // LOG_NAMED("NonholonomicAStar");

    constexpr float pi = std::numbers::pi_v<float>;
    float d = std::fmod(b - a, 2.0f * pi);
    if (d <= -pi) d += 2.0f * pi;
    if (d >   pi) d -= 2.0f * pi;
    return d;
}

// DistToPathData NonholonomicAStar::max_unimpended_dist_to_path(glm::vec3 pos, std::vector<glm::ivec3>& path, int start_id, glm::vec3 last_pos, bool replace_last_pos) {
//     LOG_METHOD();

//     int id = -1;
//     float max_dist = 0;

//     glm::vec3 cur_pos = (glm::vec3)pos;
//     for (int i = start_id; i < path.size(); i++) {
//         glm::vec3 path_pos = m_grid->voxel_center_world_pos(path[i]);

// //         if (replace_last_pos && i == path.size() - 1)
// //             path_pos = last_pos;
            

// //         path_pos.y = cur_pos.y;

//         std::vector<glm::vec3> line = {cur_pos, path_pos};

//         std::vector<glm::ivec3> ground_positions;
//         if (!m_grid->get_ground_positions(
//                 line, 
//                 ground_positions, 
//                 m_params.max_step_up, 
//                 m_params.max_drop, 
//                 m_params.max_y_diff,
//                 m_params.allow_flying_over_precipices)) {
//             continue;
//         }
        
//         id = i;
//         max_dist = glm::distance(cur_pos, path_pos); 
//     }

//     return DistToPathData{
//         .dist = max_dist,
//         .id = id
//     };
// }

int NonholonomicAStar::discretize_angle(float value, int num_bins) {
    LOG_NAMED("NonholonomicAStar");

    if (num_bins <= 0) return 0;

    const float TWO_PI = 6.2831853071795864769f;

    float a = std::fmod(value, TWO_PI);
    if (a < 0.0f) a += TWO_PI;

    float t = a / TWO_PI;
    int bin = (int)std::floor(t * num_bins + 0.5f);
    bin %= num_bins;                     
    return bin;
}

float NonholonomicAStar::get_nonholonomic_f(NonholonomicPos& new_pos, NonholonomicPos end_pos, NonholonomicPos cur_pos) {
    LOG_METHOD();

    if (m_state.unimpended_astar_positions.empty())
        return glm::distance(new_pos.pos, end_pos.pos);

    const int last_target_id = static_cast<int>(m_state.unimpended_astar_positions.size()) - 1;

    new_pos.dubins_segment_id = std::clamp(cur_pos.dubins_segment_id, 0, last_target_id);
    float dist_to_unimpended_pos = -1;
    if (m_params.allow_flying_over_precipices) {
        glm::vec3 pos1 = new_pos.pos;
        glm::vec3 pos2 = m_state.unimpended_astar_positions[new_pos.dubins_segment_id].pos;

        pos1.y = 0;
        pos2.y = 0;
        dist_to_unimpended_pos = glm::distance(pos1, pos2);
    }
    else
        dist_to_unimpended_pos = glm::distance(new_pos.pos, m_state.unimpended_astar_positions[new_pos.dubins_segment_id].pos);
    
    if (new_pos.dubins_segment_id < last_target_id && dist_to_unimpended_pos <= 1.0f) {
        new_pos.dubins_segment_id += 1;
    }

    float f = 0.0f;
    if (new_pos.dubins_segment_id < static_cast<int>(m_state.dubins_distance_to_end.size()))
        f = m_state.dubins_distance_to_end[new_pos.dubins_segment_id];

    if (new_pos.dubins_segment_id >= last_target_id) {
        std::vector<NonholonomicPathElement> dubins_path;

        if (m_params.allow_flying_over_precipices)  {
            NonholonomicPos pos1 = new_pos;
            NonholonomicPos pos2 = m_state.unimpended_astar_positions[new_pos.dubins_segment_id];

            pos1.pos.y = 0;
            pos2.pos.y = 0;

            dubins_path = ReedsShepp::get_optimal_path(pos1, pos2, m_params.min_radius);
        }
        else {
            dubins_path = ReedsShepp::get_optimal_path(new_pos, 
                m_state.unimpended_astar_positions[new_pos.dubins_segment_id], m_params.min_radius);
        }
        
        f += ReedsShepp::get_length(dubins_path) * m_params.min_radius;
        m_params.force_reeds_shepp_shot = true;

    } else if ((new_pos.dubins_segment_id > 0 && m_state.dubins_distance_to_end[new_pos.dubins_segment_id - 1] <= 2.0f) ||
               dist_to_unimpended_pos <= 2.0f) {
        f += dist_to_unimpended_pos;
    }
    else{
        std::vector<NonholonomicPathElement> dubins_path;

        if (m_params.allow_flying_over_precipices)  {
            NonholonomicPos pos1 = new_pos;
            NonholonomicPos pos2 = m_state.unimpended_astar_positions[new_pos.dubins_segment_id];

            pos1.pos.y = 0;
            pos2.pos.y = 0;

            dubins_path = ReedsShepp::get_optimal_path(pos1, pos2, m_params.min_radius);
        }
        else {
            dubins_path = ReedsShepp::get_optimal_path(new_pos, 
                m_state.unimpended_astar_positions[new_pos.dubins_segment_id], m_params.min_radius);
        }

        f += ReedsShepp::get_length(dubins_path) * m_params.min_radius;
    }

    return f;
}

std::vector<glm::ivec3> NonholonomicAStar::find_unimpended_points(
    VulkanSubmitContext& submit_context,
    const PlainAstarData& astar_path_data)
{
    LOG_METHOD();

    std::vector<glm::ivec4> astar_path;
    astar_path.reserve(astar_path_data.path.size());

    for (const glm::ivec3& point : astar_path_data.path) {
        astar_path.emplace_back(
            point.x,
            point.y,
            point.z,
            0
        );
    }

    return m_unimpened_path_finder->find_unimpended_path(
        submit_context,
        astar_path,
        m_params.max_step_up,
        m_params.max_drop,
        m_params.allow_flying_over_precipices,
        m_params.allow_diagonal_moves
    );
}

std::vector<NonholonomicPos> NonholonomicAStar::prepare_unimpended_points(
    const std::vector<glm::ivec3>& unimpended_points,
    NonholonomicPos start_pos,
    NonholonomicPos end_pos)
{
    LOG_METHOD();

    std::vector<NonholonomicPos> unimpended_path;
    unimpended_path.reserve(unimpended_points.size());

    for (const glm::ivec3& point : unimpended_points) {
        NonholonomicPos unimpended_pos;
        unimpended_pos.pos = m_grid->voxel_center_world_pos(point);
        unimpended_pos.theta = start_pos.theta;
        unimpended_path.push_back(unimpended_pos);
    }

    if (!unimpended_path.empty()) {
        unimpended_path.front().theta = start_pos.theta;
        unimpended_path.back().theta = end_pos.theta;
    }

    return unimpended_path;
}

void NonholonomicAStar::initialize(
    VulkanSubmitContext& submit_context,
    NonholonomicPos start_pos,
    NonholonomicPos end_pos)
{
    LOG_METHOD();
    m_initialize_time = AvgTimer();
    m_unimpended_path_time = AvgTimer();
    m_nonholonomic_astar_time = AvgTimer();
    m_plain_astar_time = AvgTimer();
    m_initialize_time.start();

    m_grid->clear_cache();

    m_state = NonholonomicAStarState();
    
    m_state.start_pos = start_pos;
    m_state.end_pos = end_pos;
    m_params.min_radius = m_params.wheel_base / std::tan(m_params.max_steer);

    m_plain_astar_time.start();
    m_state.plain_astar_path = m_plain_astar.find_path(
        m_grid->world_to_voxel_pos(start_pos.pos),
        m_grid->world_to_voxel_pos(end_pos.pos)
    );
    m_plain_astar_time.end();

    // logger().log() << std::to_string(m_state.plain_astar_path.path.size()) << "\n";

    if (m_state.plain_astar_path.path.empty()) {
        m_initialize_time.end();
        return; 
    }
    
    if (m_state.plain_astar_path.reached_precipice) {
        glm::vec3 pos = m_grid->voxel_center_world_pos(m_state.plain_astar_path.path.back());
        glm::vec3 dir_to_end = glm::normalize(end_pos.pos - pos);
        float theta = std::atan2(dir_to_end.z, dir_to_end.x);
        
        end_pos.pos = pos;
        end_pos.theta = theta;
    }

    m_unimpended_path_time.start();
    std::vector<glm::ivec3> unimpended_points = find_unimpended_points(submit_context, m_state.plain_astar_path);
    m_unimpended_path_time.end();

    m_state.unimpended_astar_positions = prepare_unimpended_points(unimpended_points, start_pos, end_pos);

    const float desired_distance_from_last = 5.0f;
    const float min_segment_length = 2.0f;

    if (m_state.unimpended_astar_positions.size() >= 2) {
        NonholonomicPos& previous_pos =
            m_state.unimpended_astar_positions[m_state.unimpended_astar_positions.size() - 2];
        NonholonomicPos& last_pos =
            m_state.unimpended_astar_positions[m_state.unimpended_astar_positions.size() - 1];

        const glm::vec3 segment = last_pos.pos - previous_pos.pos;
        const float segment_length = glm::length(segment);

        if (segment_length > desired_distance_from_last) {
            NonholonomicPos intermediate_pos = last_pos;

            const float previous_to_intermediate_length =
                segment_length - desired_distance_from_last;

            if (previous_to_intermediate_length < min_segment_length ||
                desired_distance_from_last < min_segment_length)
            {
                intermediate_pos.pos = (previous_pos.pos + last_pos.pos) * 0.5f;
            } else {
                intermediate_pos.pos =
                    last_pos.pos - glm::normalize(segment) * desired_distance_from_last;
            }

            m_state.unimpended_astar_positions.insert(
                m_state.unimpended_astar_positions.end() - 1,
                intermediate_pos
            );
        }
    }

    m_state.dubins_segment_lengths.reserve(m_state.unimpended_astar_positions.size() - 1);

    for (int i = 0; i < m_state.unimpended_astar_positions.size() - 1; i++) {
        NonholonomicPos cur_pos = m_state.unimpended_astar_positions[i];
        NonholonomicPos next_pos = m_state.unimpended_astar_positions[i+1];

        float dist = glm::distance(cur_pos.pos, next_pos.pos);
        if (dist > 2.0f) {
            std::vector<NonholonomicPathElement> dubins_path = ReedsShepp::get_optimal_dubins_path(
                cur_pos, next_pos, m_params.min_radius);
            dist = ReedsShepp::get_length(dubins_path) * m_params.min_radius;
        }

        m_state.dubins_segment_lengths.push_back(dist);
    }

    for (int i = 1; i < m_state.unimpended_astar_positions.size() - 1; i++) {
        
        glm::vec3 dir = glm::normalize(
            m_state.unimpended_astar_positions[i].pos - 
            m_state.unimpended_astar_positions[i-1].pos
        );
        m_state.unimpended_astar_positions[i].theta = std::atan2(dir.z, dir.x);
    }

    m_state.dubins_distance_to_end = std::vector<float>(m_state.dubins_segment_lengths.size(), 0.0f);
    float cur = 0.0f;
    for (int i = (int)m_state.dubins_segment_lengths.size() - 1; i >= 0; --i) {
        cur += m_state.dubins_segment_lengths[i];
        m_state.dubins_distance_to_end[i] = cur; 
    }

    NonholonomicAStarCell start_cell;
    start_cell.pos = start_pos;
    start_cell.no_parent = true;
    start_cell.g = 0;
    start_cell.f = 99999999;

    m_state.pq.push(start_cell);   

    // logger().log("Finished initialization");
    m_initialize_time.end();
}

bool NonholonomicAStar::try_reeds_shepp_shot(NonholonomicPos& start, NonholonomicPos& end, std::vector<NonholonomicPos>& out_path) {
    LOG_METHOD();

    const glm::vec3 voxel_size = m_grid->voxel_size();
    float line_step_world = std::min(voxel_size.x, voxel_size.z);
    if (line_step_world <= 0.0f) {
        line_step_world = m_params.reeds_shepp_step_world;
    }

    std::vector<NonholonomicPathElement> reeds_shepp_path = ReedsShepp::get_optimal_path(
        start,
        end,
        m_params.min_radius
    );

    if (reeds_shepp_path.empty()) {
        return false;
    }

    out_path = ReedsShepp::discretize_path(
        start,
        reeds_shepp_path,
        8,
        m_params.min_radius,
        std::nullopt,
        line_step_world
    );

    if (out_path.empty())
        return false;

    if (!m_grid->adjust_to_ground(
            out_path, 
            m_params.max_step_up, 
            m_params.max_drop, 
            m_params.max_y_diff, 
            m_params.allow_flying_over_precipices))
        return false;

    insert_y_transition_points(out_path);
    
    return true;
}

bool NonholonomicAStar::try_finish_with_reeds_shepp(NonholonomicPos& from, NonholonomicPos& to) {
    LOG_METHOD();

    std::vector<NonholonomicPos> reeds_shepp_path;
    if (try_reeds_shepp_shot(from, to, reeds_shepp_path)) {
        m_state.path = reconstruct_path(m_state.closed_heap, from);
        auto insert_begin = reeds_shepp_path.begin();
        if (!m_state.path.empty() && insert_begin != reeds_shepp_path.end()) {
            ++insert_begin;
        }

        m_state.path.insert(m_state.path.end(), insert_begin, reeds_shepp_path.end());
        return true;
    }
    return false;
}

bool NonholonomicAStar::find_nonholomic_path_step() {
    LOG_METHOD();

    if (m_state.pq.empty()) {
        // logger().log("1. Priotirty queue is empty");
        return true;
    }

    NonholonomicAStarCell cur_cell = m_state.pq.top();
    m_state.pq.pop();

    uint64_t cur_key = state_key(cur_cell.pos);
    auto cur_it = m_state.g_score.find(cur_key);

    if (cur_it != m_state.g_score.end()) {
        if (cur_cell.g > cur_it->second)
            return false;
    }

    if (m_params.track_explored_paths)
        if (!cur_cell.no_parent) {
            LineInstance line_instance;
            line_instance.p0 = cur_cell.pos.pos + glm::vec3(0.0f, 0.2f, 0.0f);
            line_instance.p1 = cur_cell.came_from.pos + glm::vec3(0.0f, 0.2f, 0.0f);

            m_state.explored_paths.push_back(line_instance);
        }

    if (m_state.counter >= m_params.iteration_limit) {
        // logger().log("Limit exceeded");
        return true;
    }
    
    m_state.closed_heap[cur_key] = cur_cell;

    if (almost_equal(
            cur_cell.pos, 
            m_state.end_pos,
            m_params.max_goal_position_error,
            m_params.max_goal_heading_error_radians,
            m_params.allow_flying_over_precipices)
        ) {
        m_state.path = reconstruct_path(m_state.closed_heap, cur_cell.pos);

        if (!m_params.use_reed_shepps_fallback) {
            // logger().log("2. Almost equal = true");
            return true;
        }

        bool status = try_finish_with_reeds_shepp(cur_cell.pos, m_state.end_pos);
        
        // if (status)
        //     logger().log("3. Almost equal = true (with reeds-shepp fallback)");
        
        return true;
    }

    if (m_params.use_reed_shepps_fallback || m_params.force_reeds_shepp_shot)
        if (m_state.counter % m_params.try_reeds_shepp_interval == 0 || m_params.force_reeds_shepp_shot) {
            m_params.force_reeds_shepp_shot = false;
            bool status = try_finish_with_reeds_shepp(cur_cell.pos, m_state.end_pos);
            if (status) {
                // logger().log("4. Reeds-shepp shot succeeded");
                return true;
            }
        }
    
    for (int dir = -1; dir <= 1; dir += 2)
        for (int steer = -1; steer <= 1; steer++) {
            std::vector<NonholonomicPos> motion = NonholonomicAStar::simulate_motion(cur_cell.pos, steer, dir);
            std::vector<NonholonomicPos> simplified_motion = {motion[0], motion[motion.size() - 1]};

            auto adjust_to_ground_start = std::chrono::steady_clock::now();
            if (!m_grid->adjust_to_ground(
                    simplified_motion, 
                    m_params.max_step_up, 
                    m_params.max_drop, 
                    m_params.max_y_diff, 
                    m_params.allow_flying_over_precipices)) {
                continue;
            }
                
            auto adjust_to_ground_end = std::chrono::steady_clock::now();

            std::vector<glm::ivec3> ground_positions;
            if (!m_grid->get_ground_positions(
                simplified_motion, 
                ground_positions, 
                m_params.max_step_up, 
                m_params.max_drop, 
                m_params.max_y_diff,
                m_params.allow_flying_over_precipices)) {
                continue;
            }

            float motion_dist = 0;
            for (int i = 0; i < motion.size() -1; i++) {
                motion_dist += glm::distance(motion[i].pos, motion[i+1].pos);
            }
            
            NonholonomicPos new_pos = simplified_motion[simplified_motion.size() - 1];

            uint64_t new_key = state_key(new_pos);
            auto heap_it = m_state.closed_heap.find(new_key);
            if (heap_it != m_state.closed_heap.end()) 
                continue;
            
            float new_g = cur_cell.g + motion_dist;

            auto it = m_state.g_score.find(new_key);    
            if (it != m_state.g_score.end()) {
                float old_g = it->second;
                if (old_g <= new_g)
                    continue;
            }
            
            m_state.g_score[new_key] = new_g;

            NonholonomicAStarCell new_cell;
            new_cell.pos = new_pos;
            new_cell.pos.steer = steer;
            new_cell.pos.dir = dir;
            new_cell.came_from = cur_cell.pos;
            new_cell.no_parent = false;
            new_cell.g = new_g;
            new_cell.f = new_g + get_nonholonomic_f(new_cell.pos, m_state.end_pos, cur_cell.pos);

            m_state.pq.push(new_cell);
        }
        m_state.counter++;
    return false;
}

void NonholonomicAStar::find_nonholomic_path() {
    LOG_METHOD();

    m_nonholonomic_astar_time.start();
    while (true) {
        if (find_nonholomic_path_step())
            break;
    }
    m_nonholonomic_astar_time.end();

}

OccupancyGrid3D& NonholonomicAStar::occupancy_grid() noexcept {
    return *m_grid;
}

NonholonomicAStar::NonholonomicAStarState& NonholonomicAStar::state() noexcept {
    return m_state;
}

float NonholonomicAStar::total_time_ms() {
    return initialize_time_ms() + nonholonomic_astar_time_ms();
}

float NonholonomicAStar::initialize_time_ms() {
    return static_cast<float>(m_initialize_time.total_ms());
}

float NonholonomicAStar::nonholonomic_astar_time_ms() {
    return static_cast<float>(m_nonholonomic_astar_time.total_ms());
}

float NonholonomicAStar::plain_astar_time_ms() {
    return static_cast<float>(m_plain_astar_time.total_ms());
}

float NonholonomicAStar::unimpended_path_time_ms() {
    return static_cast<float>(m_unimpended_path_time.total_ms());
}

float NonholonomicAStar::is_solid_time_ms() {
    return m_grid ? m_grid->is_solid_time_ms() : 0.0f;
}

uint32_t NonholonomicAStar::is_solid_count() {
    return m_grid ? m_grid->solid_check_count() : 0;
}
