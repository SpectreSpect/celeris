#include "grid_3d.h"

glm::ivec3 Grid3D::floor_pos(const glm::vec3& p) {
    return glm::ivec3((int)std::floor(p.x), (int)std::floor(p.y), (int)std::floor(p.z));
}

std::vector<glm::ivec3> Grid3D::line_intersects(glm::vec3 pos1, glm::vec3 pos2) {
    std::vector<glm::ivec3> out;

    glm::vec3 d = pos2 - pos1;
    double len = std::sqrt((double)d.x*d.x + (double)d.y*d.y + (double)d.z*d.z);
    if (len == 0.0) {
        out.push_back(floor_pos(pos1));
        return out;
    }

    // Adjust end slightly backward so that if pos2 is exactly on a voxel boundary,
    // we don't "accidentally" include the voxel beyond the endpoint.
    glm::vec3 dn = d / (float)len;
    glm::vec3 endp = pos2 - dn * 1e-6f;

    glm::ivec3 v    = floor_pos(pos1);
    glm::ivec3 vend = floor_pos(endp);

    // Unique (keeps traversal order) using your pack_key
    std::unordered_set<uint64_t> seen;
    seen.reserve(256); // optional hint; remove if you want

    auto push_unique = [&](const glm::ivec3& a) {
        // NOTE: pack_key throws if out of range
        uint64_t k = pack_key(a.x, a.y, a.z);
        if (seen.insert(k).second)
            out.push_back(a);
    };

    push_unique(v);
    if (v == vend) return out;

    auto sgn = [](float x) -> int { return (x > 0.f) - (x < 0.f); };

    int stepX = sgn(d.x), stepY = sgn(d.y), stepZ = sgn(d.z);

    const double INF = std::numeric_limits<double>::infinity();

    auto next_boundary = [](int cell, int step) -> double {
        // If moving +, next boundary is cell+1. If moving -, next is cell (lower face).
        return (step > 0) ? (double)(cell + 1) : (double)cell;
    };

    double tMaxX = (stepX != 0) ? (next_boundary(v.x, stepX) - (double)pos1.x) / (double)d.x : INF;
    double tMaxY = (stepY != 0) ? (next_boundary(v.y, stepY) - (double)pos1.y) / (double)d.y : INF;
    double tMaxZ = (stepZ != 0) ? (next_boundary(v.z, stepZ) - (double)pos1.z) / (double)d.z : INF;

    double tDeltaX = (stepX != 0) ? 1.0 / std::abs((double)d.x) : INF;
    double tDeltaY = (stepY != 0) ? 1.0 / std::abs((double)d.y) : INF;
    double tDeltaZ = (stepZ != 0) ? 1.0 / std::abs((double)d.z) : INF;

    // Numerical guard
    const double EPS = 1e-12;

    while (true) {
        double tNext = std::min(tMaxX, std::min(tMaxY, tMaxZ));
        if (tNext > 1.0 + EPS) break;

        bool hitX = std::abs(tMaxX - tNext) <= EPS;
        bool hitY = std::abs(tMaxY - tNext) <= EPS;
        bool hitZ = std::abs(tMaxZ - tNext) <= EPS;

        // Supercover: if we cross multiple planes at once, include the adjacent voxels
        // that share the edge/corner.
        glm::ivec3 base = v;

        if (hitX && hitY && hitZ) {
            glm::ivec3 vx   = base + glm::ivec3(stepX, 0, 0);
            glm::ivec3 vy   = base + glm::ivec3(0, stepY, 0);
            glm::ivec3 vz   = base + glm::ivec3(0, 0, stepZ);
            glm::ivec3 vxy  = base + glm::ivec3(stepX, stepY, 0);
            glm::ivec3 vxz  = base + glm::ivec3(stepX, 0, stepZ);
            glm::ivec3 vyz  = base + glm::ivec3(0, stepY, stepZ);
            glm::ivec3 vxyz = base + glm::ivec3(stepX, stepY, stepZ);

            push_unique(vx);  push_unique(vy);  push_unique(vz);
            push_unique(vxy); push_unique(vxz); push_unique(vyz);
            v = vxyz;
            push_unique(v);

            tMaxX += tDeltaX; tMaxY += tDeltaY; tMaxZ += tDeltaZ;
        } else if (hitX && hitY) {
            glm::ivec3 vx  = base + glm::ivec3(stepX, 0, 0);
            glm::ivec3 vy  = base + glm::ivec3(0, stepY, 0);
            glm::ivec3 vxy = base + glm::ivec3(stepX, stepY, 0);

            push_unique(vx); push_unique(vy);
            v = vxy;
            push_unique(v);

            tMaxX += tDeltaX; tMaxY += tDeltaY;
        } else if (hitX && hitZ) {
            glm::ivec3 vx  = base + glm::ivec3(stepX, 0, 0);
            glm::ivec3 vz  = base + glm::ivec3(0, 0, stepZ);
            glm::ivec3 vxz = base + glm::ivec3(stepX, 0, stepZ);

            push_unique(vx); push_unique(vz);
            v = vxz;
            push_unique(v);

            tMaxX += tDeltaX; tMaxZ += tDeltaZ;
        } else if (hitY && hitZ) {
            glm::ivec3 vy  = base + glm::ivec3(0, stepY, 0);
            glm::ivec3 vz  = base + glm::ivec3(0, 0, stepZ);
            glm::ivec3 vyz = base + glm::ivec3(0, stepY, stepZ);

            push_unique(vy); push_unique(vz);
            v = vyz;
            push_unique(v);

            tMaxY += tDeltaY; tMaxZ += tDeltaZ;
        } else if (hitX) {
            v.x += stepX;
            push_unique(v);
            tMaxX += tDeltaX;
        } else if (hitY) {
            v.y += stepY;
            push_unique(v);
            tMaxY += tDeltaY;
        } else { // hitZ
            v.z += stepZ;
            push_unique(v);
            tMaxZ += tDeltaZ;
        }

        if (v == vend) break;
    }

    return out;
}

// glm::ivec2 Grid3D::floor_pos2(const glm::vec2& p) {
//     return glm::ivec2((int)std::floor(p.x), (int)std::floor(p.y));
// }

// // Simple 2D pack into 64 bits (works for signed 32-bit coords, collision-free)
// static inline uint64_t pack_key2(int32_t x, int32_t y) {
//     return (uint64_t)(uint32_t)x << 32 | (uint32_t)y;
// }

std::vector<glm::ivec3> Grid3D::line_intersects_xz(glm::vec3 pos1, glm::vec3 pos2) {
    std::vector<glm::ivec3> out;

    // Work in 2D (x,z), but output ivec3 with a fixed y layer.
    glm::vec2 p1(pos1.x, pos1.z);
    glm::vec2 p2(pos2.x, pos2.z);
    glm::vec2 d = p2 - p1;

    double len = std::sqrt((double)d.x*d.x + (double)d.y*d.y);
    int y_layer = (int)std::floor(pos1.y);

    if (len == 0.0) {
        glm::ivec3 v0((int)std::floor(pos1.x), y_layer, (int)std::floor(pos1.z));
        out.push_back(v0);
        return out;
    }

    // Pull end slightly back to avoid including cell beyond endpoint when exactly on boundary.
    glm::vec2 dn = d / (float)len;
    glm::vec2 endp2 = p2 - dn * 1e-6f;

    glm::ivec2 v2((int)std::floor(p1.x), (int)std::floor(p1.y));
    glm::ivec2 vend2((int)std::floor(endp2.x), (int)std::floor(endp2.y));

    // Unique (keeps traversal order) using your 3D pack_key (y fixed)
    std::unordered_set<uint64_t> seen;
    seen.reserve(128);

    auto push_unique = [&](const glm::ivec2& a2) {
        glm::ivec3 a3(a2.x, y_layer, a2.y); // note: a2.y is "z"
        uint64_t k = pack_key(a3.x, a3.y, a3.z); // may throw if out of range
        if (seen.insert(k).second)
            out.push_back(a3);
    };

    push_unique(v2);
    if (v2 == vend2) return out;

    auto sgn = [](float x) -> int { return (x > 0.f) - (x < 0.f); };
    int stepX = sgn(d.x);
    int stepZ = sgn(d.y); // d.y is actually deltaZ

    const double INF = std::numeric_limits<double>::infinity();

    auto next_boundary = [](int cell, int step) -> double {
        return (step > 0) ? (double)(cell + 1) : (double)cell;
    };

    // t in [0,1] param along segment in XZ plane:
    double tMaxX = (stepX != 0) ? (next_boundary(v2.x, stepX) - (double)p1.x) / (double)d.x : INF;
    double tMaxZ = (stepZ != 0) ? (next_boundary(v2.y, stepZ) - (double)p1.y) / (double)d.y : INF;

    double tDeltaX = (stepX != 0) ? 1.0 / std::abs((double)d.x) : INF;
    double tDeltaZ = (stepZ != 0) ? 1.0 / std::abs((double)d.y) : INF;

    const double EPS = 1e-12;

    while (true) {
        double tNext = std::min(tMaxX, tMaxZ);
        if (tNext > 1.0 + EPS) break;

        bool hitX = std::abs(tMaxX - tNext) <= EPS;
        bool hitZ = std::abs(tMaxZ - tNext) <= EPS;

        glm::ivec2 base = v2;

        if (hitX && hitZ) {
            // Corner hit => supercover
            glm::ivec2 vx  = base + glm::ivec2(stepX, 0);
            glm::ivec2 vz  = base + glm::ivec2(0, stepZ);
            glm::ivec2 vxz = base + glm::ivec2(stepX, stepZ);

            push_unique(vx);
            push_unique(vz);
            v2 = vxz;
            push_unique(v2);

            tMaxX += tDeltaX;
            tMaxZ += tDeltaZ;
        } else if (hitX) {
            v2.x += stepX;
            push_unique(v2);
            tMaxX += tDeltaX;
        } else { // hitZ
            v2.y += stepZ;
            push_unique(v2);
            tMaxZ += tDeltaZ;
        }

        if (v2 == vend2) break;
    }

    return out;
}

bool Grid3D::adjust_to_ground(std::vector<glm::vec3>& output, int max_step_up, int max_drop, int max_y_diff) {
    bool ok = adjust_to_ground_range(output.data(), 
                                          output.data() + output.size(), 
                                          [](const glm::vec3& s){ return s;},
                                          [](glm::vec3& s, const glm::vec3& p){ s.y = p.y; },
                                          max_step_up, max_drop, max_y_diff);
    
    // for (int i = 0; i < output.size(); i++) {
    //     glm::vec3 result_pos = output[i];
    //     if (!adjust_to_ground(result_pos, max_step_up, max_drop, max_y_diff)) {
    //         return false;
    //     }
            
        
    //     if (max_y_diff >= 0)
    //         if (std::abs(output[i].y - result_pos.y) > max_y_diff) {
    //             return false;
    //         }
                
        
    //     output[i].y = result_pos.y;
    // }
    // return true;
    return ok;
}

bool Grid3D::adjust_to_ground(std::vector<glm::ivec3>& output, int max_step_up, int max_drop, int max_y_diff) {
    bool ok = adjust_to_ground_range(output.data(), 
                                          output.data() + output.size(), 
                                          [](const glm::ivec3& s){ return (glm::vec3)s;},
                                          [](glm::ivec3& s, const glm::vec3& p){ s.y = p.y; },
                                          max_step_up, max_drop, max_y_diff);
    
    
    // for (int i = 0; i < output.size(); i++) {
    //     glm::vec3 result_pos = output[i];
    //     if (!adjust_to_ground(result_pos, max_step_up, max_drop))
    //         return false;
        
    //     if (max_y_diff >= 0)
    //         if (std::abs(output[i].y - result_pos.y) > max_y_diff)
    //             return false;
        
    //     output[i].y = result_pos.y;
    // }
    // return true;

    return ok;
}

bool Grid3D::adjust_to_ground(std::vector<NonholonomicPos>& output, int max_step_up, int max_drop, int max_y_diff) {
    bool ok = adjust_to_ground_range(output.data(), 
                                     output.data() + output.size(), 
                                     [](const NonholonomicPos& s){ return (glm::vec3)s.pos;},
                                     [](NonholonomicPos& s, const glm::vec3& p){ s.pos.y = p.y; },
                                     max_step_up, max_drop, max_y_diff);
    // for (int i = 0; i < output.size(); i++) {
    //     glm::vec3 result_pos = output[i].pos;
    //     if (!adjust_to_ground(result_pos, max_step_up, max_drop))
    //         return false;
        
    //     if (max_y_diff >= 0)
    //         if (std::abs(output[i].pos.y - result_pos.y) > max_y_diff)
    //             return false;
        
    //     output[i].pos.y = result_pos.y;
    // }
    // return true;
    return ok;
}

bool Grid3D::adjust_to_ground(glm::vec3& output, int max_step_up, int max_drop, int max_y_diff) {
    glm::ivec3 norm_pos = glm::ivec3(glm::floor(output));
    glm::ivec3 result_pos = norm_pos;

    if(!get_closest_visible_bottom_pos(norm_pos, result_pos, max_drop)) {
        return false; // couldn't find
    }


    if ((int)norm_pos.y == (int)result_pos.y) {
        if (!get_closest_invisible_top_pos(norm_pos + glm::ivec3(0, 1, 0), result_pos, max_step_up)) {
            return false; // couldn't find
        }
    }
    else
        result_pos += glm::ivec3(0, 1, 0); // first invisible pos
    
    if (max_y_diff >= 0) {
        float diff = std::abs(result_pos.y - norm_pos.y);
        if (diff > max_y_diff) {
            return false;
        }
    }
    
    output.y = result_pos.y;
    return true;
}

bool Grid3D::get_closest_invisible_top_pos(glm::ivec3 pos, glm::ivec3 &result, int scan_height) {
    for (int y = 0; y <= scan_height; y++) {
        glm::ivec3 cur_pos = pos + glm::ivec3(0, y, 0);
        // Voxel voxel = get_voxel(cur_pos);
        if (!is_solid(cur_pos)) {
            result = cur_pos;
            return true;
        }
    }
    return false;
}

bool Grid3D::get_closest_visible_bottom_pos(glm::ivec3 pos, glm::ivec3 &result, int max_drop) {
    for (int y = 0; y <= max_drop; y++) {
        glm::ivec3 cur_pos = pos - glm::ivec3(0, y, 0);
        // Voxel voxel = get_voxel(cur_pos);
        if (is_solid(cur_pos)) {
            result = cur_pos;
            return true;
        }
    }
    return false;
}

bool Grid3D::get_ground_positions(glm::vec3 pos1, glm::vec3 pos2, std::vector<glm::ivec3>& output, int max_step_up, int max_drop, int max_y_diff) {
    // std::vector<glm::ivec3> line_positions = line_intersects(pos1, pos2);
    std::vector<glm::ivec3> line_positions = line_intersects_xz(pos1, pos2);
    
    if (!adjust_to_ground(line_positions, max_step_up, max_drop, max_y_diff))
        return false;
    
    for (int i = 0; i < line_positions.size(); i++)
        line_positions[i].y -= 1;

    output.insert(output.end(), line_positions.begin(), line_positions.end());

    return true;
}

bool Grid3D::get_ground_positions(std::vector<glm::vec3> polyline, std::vector<glm::ivec3>& output, int max_step_up, int max_drop, int max_y_diff) {
    if (polyline.size() < 2)
        return false;
       
    for (int i = 0; i < polyline.size() - 1; i++) {
        if (!get_ground_positions(polyline[i], polyline[i+1], output, max_step_up, max_drop, max_y_diff))
            return false;
    }
    
    return true;
}

bool Grid3D::get_ground_positions(std::vector<NonholonomicPos> polyline, std::vector<glm::ivec3>& output, int max_step_up, int max_drop, int max_y_diff) {
    if (polyline.size() < 2)
        return false;
       
    for (int i = 0; i < polyline.size() - 1; i++) {
        if (!get_ground_positions(polyline[i].pos, polyline[i+1].pos, output, max_step_up, max_drop, max_y_diff))
            return false;
    }
    
    return true;
}