#pragma once
#include <unordered_map>
#include <cstdint>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "nonholonomic_pos.h"
#include "transformable.h"

constexpr int BITS = 21;
constexpr uint64_t MASK = (1ull << BITS) - 1; // 0x1FFFFF
constexpr int OFFSET = (1 << (BITS-1)); // offset to encode signed -> unsigned

class Grid3D : public Transformable {
public:
    static glm::ivec3 floor_pos(const glm::vec3& p);
    static std::vector<glm::ivec3> line_intersects(glm::vec3 pos1, glm::vec3 pos2);    
    virtual bool is_solid(glm::ivec3 pos) = 0;
    bool adjust_to_ground(std::vector<glm::vec3>& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1);
    bool adjust_to_ground(std::vector<glm::ivec3>& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1);
    bool adjust_to_ground(std::vector<NonholonomicPos>& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1);
    bool adjust_to_ground(glm::vec3& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1);
    bool get_closest_invisible_top_pos(glm::ivec3 pos, glm::ivec3 &result, int scan_height);
    bool get_closest_visible_bottom_pos(glm::ivec3 pos, glm::ivec3 &result, int max_drop);
    bool get_ground_positions(glm::vec3 pos1, glm::vec3 pos2, std::vector<glm::ivec3>& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1);
    bool get_ground_positions(std::vector<glm::vec3> polyline, std::vector<glm::ivec3>& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1);
    bool get_ground_positions(std::vector<NonholonomicPos> polyline, std::vector<glm::ivec3>& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1);
    
    // glm::ivec2 floor_pos2(const glm::vec2& p);
    // static uint64_t pack_key2(int32_t x, int32_t y);
    std::vector<glm::ivec3> line_intersects_xz(glm::vec3 pos1, glm::vec3 pos2);


    template <class T, class GetPos, class SetPos>
    bool adjust_to_ground_range(T* begin, T* end,
                                        GetPos get_pos, SetPos set_pos,
                                        int max_step_up, int max_drop, int max_y_diff)
    {
        if (begin == end) return true; 

        float last_y = get_pos(*begin).y;

        for (auto* it = begin; it != end; ++it) {
            glm::vec3 p = get_pos(*it);
            p.y = last_y;

            if (!adjust_to_ground(p, max_step_up, max_drop, max_y_diff))
                return false;

            // if (max_y_diff >= 0 && std::abs(get_pos(*it).y - p.y) > max_y_diff)
            //     return false;

            set_pos(*it, p);
            last_y = p.y;
        }
        return true;
    }

    static uint64_t pack_key(int32_t cx, int32_t cy, int32_t cz) {
        static_assert(BITS > 0 && 3 * BITS <= 64);

        if (cx < -OFFSET || cx > OFFSET - 1 ||
            cy < -OFFSET || cy > OFFSET - 1 ||
            cz < -OFFSET || cz > OFFSET - 1) {
            throw std::out_of_range("chunk coord out of packable range");
        }

        auto enc = [](int32_t c) -> uint64_t {
            uint64_t u = static_cast<uint64_t>(static_cast<int64_t>(c) + OFFSET);
            return u & MASK;
        };

        uint64_t ux = enc(cx);
        uint64_t uy = enc(cy);
        uint64_t uz = enc(cz);

        return (ux << (BITS * 2)) | (uy << BITS) | uz;
    }

    static glm::vec3 unpack_key(uint64_t key) {
        uint64_t ux = (key >> (BITS*2)) & MASK;
        uint64_t uy = (key >> BITS) & MASK;
        uint64_t uz = key & MASK;
        int32_t cx = (int)( (int)ux - OFFSET );
        int32_t cy = (int)( (int)uy - OFFSET );
        int32_t cz = (int)( (int)uz - OFFSET );

        return glm::ivec3(cx, cy, cz);
    }

    template <class It, class GetPos, class Fn>
    bool for_each_ground_cell_on_polyline_xz(It begin, It end,
                                            GetPos getPos,
                                            int max_step_up, int max_drop, int max_y_diff,
                                            Fn&& fn) const
    {
        if (begin == end) return false;
        It it0 = begin;
        It it1 = std::next(begin);
        if (it1 == end) return false;

        glm::ivec3 last_sent(0);
        bool have_last = false;

        for (; it1 != end; ++it0, ++it1) {
            glm::vec3 p0 = getPos(*it0);
            glm::vec3 p1 = getPos(*it1);

            const int y_layer = (int)std::floor(p0.y);

            bool ok_seg = detail_for_each_cell_on_line_xz(p0, p1, [&](const glm::ivec2& c2) -> bool {
                // Probe position in this XZ cell at the segment's y layer
                glm::vec3 probe((float)c2.x, (float)y_layer, (float)c2.y);

                // Adjust probe.y to the first "invisible" top position (your convention)
                if (!const_cast<Grid3D*>(this)->adjust_to_ground(probe, max_step_up, max_drop, max_y_diff))
                    return false;

                // Convert to ground cell (same as your get_ground_positions(): y -= 1)
                glm::ivec3 ground(c2.x, (int)probe.y - 1, c2.y);

                // Skip trivial duplicates at polyline joints
                if (have_last && ground == last_sent) return true;
                last_sent = ground;
                have_last = true;

                // User callback decides whether to keep going
                return fn(ground);
            });

            if (!ok_seg) return false;
        }

        return true;
    }

    // Convenience overload for vector<glm::vec3>
    template <class Fn>
    bool for_each_ground_cell_on_polyline_xz(const std::vector<glm::vec3>& polyline,
                                            int max_step_up, int max_drop, int max_y_diff,
                                            Fn&& fn) const
    {
        return for_each_ground_cell_on_polyline_xz(
            polyline.begin(), polyline.end(),
            [](const glm::vec3& p) { return p; },
            max_step_up, max_drop, max_y_diff,
            std::forward<Fn>(fn)
        );
    }
private:
    template <class FnCell>
    static bool detail_for_each_cell_on_line_xz(glm::vec3 pos1, glm::vec3 pos2, FnCell&& fn)
    {
        // Pack x,z into 64-bit for local uniqueness
        auto pack_key2 = [](int32_t x, int32_t z) -> uint64_t {
            return (uint64_t)(uint32_t)x << 32 | (uint32_t)z;
        };

        glm::vec2 p1(pos1.x, pos1.z);
        glm::vec2 p2(pos2.x, pos2.z);
        glm::vec2 d = p2 - p1;

        double len = std::sqrt((double)d.x * (double)d.x + (double)d.y * (double)d.y);
        if (len == 0.0) {
            glm::ivec2 v((int)std::floor(p1.x), (int)std::floor(p1.y));
            return fn(v);
        }

        // Pull end slightly back to avoid including the cell beyond endpoint on boundary hits
        glm::vec2 dn = d / (float)len;
        glm::vec2 endp = p2 - dn * 1e-6f;

        glm::ivec2 v((int)std::floor(p1.x), (int)std::floor(p1.y));
        glm::ivec2 vend((int)std::floor(endp.x), (int)std::floor(endp.y));

        std::unordered_set<uint64_t> seen;
        seen.reserve(128);

        auto push_unique = [&](const glm::ivec2& c) -> bool {
            uint64_t k = pack_key2(c.x, c.y);
            if (seen.insert(k).second) {
                return fn(c);
            }
            return true; // already seen -> continue
        };

        if (!push_unique(v)) return false;
        if (v == vend) return true;

        auto sgn = [](float x) -> int { return (x > 0.f) - (x < 0.f); };
        int stepX = sgn(d.x);
        int stepZ = sgn(d.y); // d.y is deltaZ in XZ plane

        const double INF = std::numeric_limits<double>::infinity();

        auto next_boundary = [](int cell, int step) -> double {
            return (step > 0) ? (double)(cell + 1) : (double)cell;
        };

        double tMaxX = (stepX != 0) ? (next_boundary(v.x, stepX) - (double)p1.x) / (double)d.x : INF;
        double tMaxZ = (stepZ != 0) ? (next_boundary(v.y, stepZ) - (double)p1.y) / (double)d.y : INF;

        double tDeltaX = (stepX != 0) ? 1.0 / std::abs((double)d.x) : INF;
        double tDeltaZ = (stepZ != 0) ? 1.0 / std::abs((double)d.y) : INF;

        const double EPS = 1e-12;

        while (true) {
            double tNext = std::min(tMaxX, tMaxZ);
            if (tNext > 1.0 + EPS) break;

            bool hitX = std::abs(tMaxX - tNext) <= EPS;
            bool hitZ = std::abs(tMaxZ - tNext) <= EPS;

            glm::ivec2 base = v;

            if (hitX && hitZ) {
                // Corner hit => supercover
                glm::ivec2 vx  = base + glm::ivec2(stepX, 0);
                glm::ivec2 vz  = base + glm::ivec2(0, stepZ);
                glm::ivec2 vxz = base + glm::ivec2(stepX, stepZ);

                if (!push_unique(vx))  return false;
                if (!push_unique(vz))  return false;
                v = vxz;
                if (!push_unique(v))   return false;

                tMaxX += tDeltaX;
                tMaxZ += tDeltaZ;
            } else if (hitX) {
                v.x += stepX;
                if (!push_unique(v)) return false;
                tMaxX += tDeltaX;
            } else { // hitZ
                v.y += stepZ;
                if (!push_unique(v)) return false;
                tMaxZ += tDeltaZ;
            }

            if (v == vend) break;
        }

        return true;
    }
};