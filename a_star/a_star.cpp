#include "a_star.h"

#include <cmath>

AStar::AStar(OccupancyGrid3D& occupancy_grid, const AStarDesc& desc) 
    :   m_grid(&occupancy_grid),
        m_params(desc) {}




// float AStar::get_heuristic(glm::ivec3 a, glm::ivec3 b) {
//     return glm::distance(glm::vec3(a), glm::vec3(b));
// }

float AStar::get_heuristic(glm::ivec3 a, glm::ivec3 b)
{
    float dx = std::abs(a.x - b.x);
    float dz = std::abs(a.z - b.z);

    if (!m_params.allow_diagonal_moves)
        return dx + dz; // Manhattan distance

    float diagonal = std::min(dx, dz);
    float straight = std::max(dx, dz) - diagonal;

    return diagonal * std::sqrt(2.0f) + straight; // Octile distance
}

std::vector<glm::ivec3> AStar::get_straight_path(glm::ivec3& start, glm::ivec3& end, std::vector<glm::ivec3>& out_path) {
    LOG_METHOD();

    glm::ivec3 delta = end - start;
    int steps = std::max({std::abs(delta.x), std::abs(delta.y), std::abs(delta.z)});

    out_path.clear();
    out_path.reserve(steps + 1);

    if (steps == 0) {
        out_path.push_back(start);
        return out_path;
    }

    glm::vec3 start_f = glm::vec3(start);
    glm::vec3 step = glm::vec3(delta) / static_cast<float>(steps);

    for (int i = 0; i <= steps; i++) {
        glm::vec3 p = start_f + step * static_cast<float>(i);
        out_path.push_back(glm::ivec3(glm::round(p)));
    }

    return out_path;
}

bool AStar::try_straight_shot(glm::ivec3& start, glm::ivec3& end, std::vector<glm::ivec3>& out_path) {
    LOG_METHOD();

    get_straight_path(start, end, out_path);

    if (out_path.empty())
        return false;

    if (!m_grid->adjust_to_ground(out_path, m_params.max_step_up, m_params.max_drop, m_params.max_y_diff))
        return false;
    
    return true;
}

PlainAstarData AStar::reconstruct_path(std::unordered_map<uint64_t, AStarCell> closed_heap, glm::ivec3 pos) {
    LOG_METHOD();
    
    PlainAstarData plain_astar_data;
    glm::ivec3 cur_pos = pos;
    float dist_to_end = 0;

    while (true) {
        uint64_t cur_key = math_utils::pack_key(cur_pos.x, cur_pos.y, cur_pos.z);

        auto it = closed_heap.find(cur_key);

        if (it == closed_heap.end())
            return {};
            
        AStarCell prev_cell = it->second;
        
        dist_to_end += glm::distance((glm::vec3)cur_pos, (glm::vec3)prev_cell.came_from);
            
        plain_astar_data.path.push_back(cur_pos);
        plain_astar_data.dist_to_end.push_back(dist_to_end);
        cur_pos = prev_cell.came_from;

        if (prev_cell.no_parent)
            break;
    }

    std::reverse(plain_astar_data.path.begin(), plain_astar_data.path.end());
    std::reverse(plain_astar_data.dist_to_end.begin(), plain_astar_data.dist_to_end.end());

    return plain_astar_data;
}

PlainAstarData AStar::find_path(glm::ivec3 start_pos, glm::ivec3 end_pos) {
    LOG_METHOD();

    std::priority_queue<AStarCell, std::vector<AStarCell>, ByPriority> pq;
    std::unordered_map<uint64_t, AStarCell> closed_heap;
    std::unordered_map<uint64_t, float> g_score;
    
    AStarCell start;
    start.pos = start_pos;
    start.no_parent = true;
    start.g = 0;
    start.f = get_heuristic(start_pos, end_pos);

    int counter = 0;

    pq.push(start);

    while (!pq.empty()) {
        AStarCell cur_cell = pq.top();
        pq.pop();

        if (counter >= m_params.iteration_limit)
            return {};
        
        uint64_t cur_key = math_utils::pack_key(cur_cell.pos.x, cur_cell.pos.y, cur_cell.pos.z);
        auto cur_it = g_score.find(cur_key);

        if (cur_it != g_score.end())
            if (cur_cell.g > cur_it->second)
                continue;
        
        closed_heap[cur_key] = cur_cell;

        // if (m_params.use_straight_fallback && counter % m_params.try_straight_interval == 0) {
        //     std::vector<glm::ivec3> out_path;
        //     if (try_straight_shot(cur_cell.pos, end_pos, out_path)) {
        //         PlainAstarData data = reconstruct_path(closed_heap, cur_cell.pos);

        //         if (out_path.size() == 1)
        //             return data;

        //         float dist_to_end = data.dist_to_end.back();
        //         glm::ivec3 prev_pos = data.path.back();

        //         for (int i = 1; i < out_path.size(); i++) {
        //             glm::ivec3& cur_pos = out_path[i];

        //             dist_to_end += glm::distance((glm::vec3)cur_pos, (glm::vec3)prev_pos);
                    
        //             data.path.push_back(cur_pos);
        //             data.dist_to_end.push_back(dist_to_end);
                    
        //             prev_pos = out_path[i];
        //         }

        //         return data;
        //     }
        // }

        counter++;

        // if (cur_cell.pos == end_pos) {
        //     return reconstruct_path(closed_heap, cur_cell.pos);
        // }

        bool reached_goal = false;

        if (m_params.allow_flying_over_precepices)
            reached_goal = (cur_cell.pos.x == end_pos.x) && (cur_cell.pos.z == end_pos.z);
        else
            reached_goal = cur_cell.pos == end_pos;

        if (reached_goal) {
            return reconstruct_path(closed_heap, cur_cell.pos);
        }
            
        for (int dx = -1; dx <= 1; dx++)
            for (int dz = -1; dz <= 1; dz++) {

                if (dx == 0 && dz == 0)
                    continue;
                
                if (!m_params.allow_diagonal_moves) {
                    if (dx != 0 && dz != 0)
                        continue;
                }

                int nx = dx + cur_cell.pos.x;
                int ny = cur_cell.pos.y;
                int nz = dz + cur_cell.pos.z;

                glm::ivec3 new_pos(nx, ny, nz);

                uint32_t status = 0;

                // int cube_radius = 2;

                // bool should_continue = false;
                // for (int xc = -cube_radius; xc < cube_radius; xc++) {
                //     for (int zc = -cube_radius; zc < cube_radius; zc++) {
                //         glm::vec3 test_pos = new_pos + glm::vec3(xc, 0, zc);

                //         if (!m_grid->adjust_to_ground(
                //             test_pos, 
                //             m_params.max_step_up, 
                //             m_params.max_drop, 
                //             m_params.max_y_diff, 
                //             m_params.allow_flying_over_precepices, 
                //             &status)
                //         ) {
                //             should_continue = true;
                //             break;
                //         }
                //     }
                //     if (should_continue)
                //         break;
                // }

                // if (should_continue)
                //     continue;
                

                if (!m_grid->adjust_to_ground(
                        new_pos, 
                        m_params.max_step_up, 
                        m_params.max_drop, 
                        m_params.max_y_diff, 
                        m_params.allow_flying_over_precepices, 
                        &status)
                    ) {
                    continue;
                }

                // int cube_radius = 5;

                // bool footprint_result = m_grid->check_footprint(
                //     new_pos - glm::vec3(cube_radius, 0, cube_radius),
                //     glm::vec3(cube_radius * 2, cube_radius * 2, cube_radius * 2),
                //     1);
                
                // if (!footprint_result)
                //     continue;

                uint64_t new_key = math_utils::pack_key(new_pos.x, new_pos.y, new_pos.z);
                auto heap_it = closed_heap.find(new_key);
                if (heap_it != closed_heap.end())
                    continue;

                float new_g = cur_cell.g + glm::distance((glm::vec3)cur_cell.pos, (glm::vec3)new_pos);

                uint64_t key = math_utils::pack_key(new_pos.x, new_pos.y, new_pos.z);
                auto it = g_score.find(key);
                
                if (it != g_score.end()) {
                    float old_g = it->second;
                    if (old_g <= new_g)
                        continue;
                }

                g_score[key] = new_g;

                AStarCell new_cell;
                new_cell.pos = new_pos;
                new_cell.came_from = cur_cell.pos;
                new_cell.no_parent = false;
                new_cell.g = new_g;
                
                new_cell.f = new_g + m_params.heuristic_weight * get_heuristic(new_pos, end_pos);

                pq.push(new_cell);
            }
    }
    return {};
}

OccupancyGrid3D& AStar::occupancy_grid() noexcept {
    return *m_grid;
}
