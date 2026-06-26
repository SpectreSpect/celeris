#include "curvature_a_star.h"


PlainAstarData CurvatureAStar::reconstruct_path(std::unordered_map<uint64_t, AStarCell> closed_heap, glm::ivec3 pos) {
    PlainAstarData plain_astar_data;
    // plain_astar_data.path.push_back(pos);
    glm::ivec3 cur_pos = pos;
    float dist_to_end = 0;

    while (true) {
        uint64_t cur_key = grid->pack_key(cur_pos.x, cur_pos.y, cur_pos.z);
        auto it = closed_heap.find(cur_key);

        if (it == closed_heap.end())
            return {};
            
        AStarCell prev_cell = it->second;

        dist_to_end += glm::distance((glm::vec3)cur_pos, (glm::vec3)prev_cell.came_from);
            
        plain_astar_data.path.push_back(cur_pos);
        plain_astar_data.dist_to_end.push_back(dist_to_end);


        if (prev_cell.has_intermediate_pos) {
            dist_to_end += glm::distance((glm::vec3)cur_pos, (glm::vec3)prev_cell.intermediate_pos);

            plain_astar_data.path.push_back(prev_cell.intermediate_pos);
            plain_astar_data.dist_to_end.push_back(dist_to_end);
        }

        cur_pos = prev_cell.came_from;

        if (prev_cell.no_parent)
            break;
    }

    std::reverse(plain_astar_data.path.begin(), plain_astar_data.path.end());
    std::reverse(plain_astar_data.dist_to_end.begin(), plain_astar_data.dist_to_end.end());

    return plain_astar_data;
}


PlainAstarData CurvatureAStar::find_path(glm::ivec3 start_pos, glm::ivec3 end_pos) {
    std::priority_queue<AStarCell, std::vector<AStarCell>, ByPriority> pq;
    std::unordered_map<uint64_t, AStarCell> closed_heap;
    std::unordered_map<uint64_t, float> g_score;
    
    AStarCell start;
    start.pos = start_pos;
    start.no_parent = true;
    start.g = 0;
    start.f = 0;

    int limit = 10000;
    int counter = 0;

    pq.push(start);

    while (!pq.empty()) {
        AStarCell cur_cell = pq.top();
        pq.pop();

        if (counter >= limit) {
            std::cout << "LIMIT EXCEEDED" << std::endl;
            return {};
        }

        uint64_t cur_key = grid->pack_key(cur_cell.pos.x, cur_cell.pos.y, cur_cell.pos.z);
        auto cur_it = g_score.find(cur_key);

        if (cur_it != g_score.end())
            if (cur_cell.g > cur_it->second)
                continue;
        
        closed_heap[cur_key] = cur_cell;

        counter++;
        if (cur_cell.pos == end_pos)
            return reconstruct_path(closed_heap, cur_cell.pos);
            
        for (int dx = -1; dx <= 1; dx++)
            for (int dz = -1; dz <= 1; dz++) {

                if (dx == 0 && dz == 0)
                    continue;
                 
                if (!allow_diagonal_moves) {
                    if (dx != 0 && dz != 0)
                        continue;
                }

                int nx = dx + cur_cell.pos.x;
                int ny = cur_cell.pos.y;
                int nz = dz + cur_cell.pos.z;

                glm::vec3 new_pos = glm::vec3(nx, ny, nz);

                if (!grid->adjust_to_ground(new_pos, max_step_up, max_drop, max_y_diff))
                    continue;

                glm::vec3 intermediate_pos;
                bool has_intermediate_pos = false;
                // if (dx != 0 && dz != 0) {
                //     glm::vec3 intermediate_pos_1 = glm::vec3(cur_cell.pos.x, ny, nz);
                //     bool ok_1 = grid->adjust_to_ground(intermediate_pos_1, max_step_up, max_drop, max_y_diff);

                //     intermediate_pos = intermediate_pos_1;
                //     if (!ok_1) {
                //         glm::vec3 intermediate_pos_2 = glm::vec3(nx, ny, cur_cell.pos.z);
                //         bool ok_2 = grid->adjust_to_ground(intermediate_pos_2, max_step_up, max_drop, max_y_diff);

                //         if (!ok_2)
                //             continue;
                        
                //         intermediate_pos = intermediate_pos_2;
                //     }
                //     has_intermediate_pos = true;
                // }


                //     OccupancyCell intermediate_ground_occupancy_cell = grid->get_cell(intermediate_pos - glm::vec3(0, 1, 0));
                //     if (intermediate_ground_occupancy_cell.curvature >= curvature_limit)
                //         continue; 
                // }

                // bool should_continue = false;
                // float radius = 2.5f;
                // for (int circle_x = new_pos.x - radius; circle_x <= new_pos.x + radius; circle_x++) {
                //     for (int circle_z = new_pos.z - radius; circle_z <= new_pos.z + radius; circle_z++) {
                //         glm::vec3 cicle_point = glm::vec3(circle_x, ny, circle_z);

                //         if (glm::distance(cicle_point, new_pos) > radius)
                //             continue;

                //         if (!grid->adjust_to_ground(cicle_point, max_step_up, max_drop, max_y_diff)) {
                //             should_continue = true;
                //             break;
                //         }

                //         OccupancyCell circle_ground_occupancy_cell = grid->get_cell(cicle_point - glm::vec3(0, 1, 0));
                //         if (circle_ground_occupancy_cell.curvature >= curvature_limit) {
                //             should_continue = true;
                //             break;
                //         }
                //     }
                //     if (should_continue)
                //         break;
                // }

                // if (should_continue)
                //     continue;


                OccupancyCell ground_occupancy_cell = grid->get_cell(new_pos - glm::vec3(0, 1, 0));
                if (ground_occupancy_cell.curvature >= curvature_limit)
                    continue;
                
                

                uint64_t new_key = grid->pack_key(new_pos.x, new_pos.y, new_pos.z);
                auto heap_it = closed_heap.find(new_key);
                if (heap_it != closed_heap.end()) 
                    continue;
                    
                
                float new_g = cur_cell.g + glm::distance((glm::vec3)cur_cell.pos, (glm::vec3)new_pos);

                uint64_t key = grid->pack_key(new_pos.x, new_pos.y, new_pos.z);
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

                if (has_intermediate_pos) {
                    new_cell.has_intermediate_pos = has_intermediate_pos;
                    new_cell.intermediate_pos = intermediate_pos;
                }
                
                new_cell.f = new_g + get_heuristic(new_pos, end_pos);

                pq.push(new_cell);
            }
    }
    return {};
}