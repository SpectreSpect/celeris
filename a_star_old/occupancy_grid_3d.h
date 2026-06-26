#pragma once
#include <unordered_map>
#include "occupancy_cell.h"
#include <cstdint>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <iostream>
#include <vector>
#include "../grid_3d.h"

// constexpr int BITS2 = 21;
// constexpr uint64_t MASK2 = (1ull << BITS2) - 1; // 0x1FFFFF
// constexpr int OFFSET2 = (1 << (BITS2-1)); // offset to encode signed -> unsigned

class OccupancyGrid3D : public Grid3D {
public:
    std::unordered_map<uint64_t, OccupancyCell> occupancy_cells;
    // std::unordered_map<uint64_t, OccupancyCell> extra_cells;
    virtual bool is_solid(glm::ivec3 pos) override;
    virtual void set_cell(glm::ivec3 pos, OccupancyCell occupancy_cell);
    virtual OccupancyCell get_cell(glm::ivec3 pos);
    // virtual bool adjust_to_ground(glm::vec3& pos, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1);
    // virtual bool adjust_to_ground(std::vector<glm::vec3>& pos, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1);
    // virtual bool get_ground_positions(glm::vec3 pos1, glm::vec3 pos2, std::vector<glm::ivec3>& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1);
    

    // static uint64_t pack_key(int32_t cx, int32_t cy, int32_t cz) {
    //     static_assert(BITS2 > 0 && 3 * BITS2 <= 64);

    //     if (cx < -OFFSET2 || cx > OFFSET2 - 1 ||
    //         cy < -OFFSET2 || cy > OFFSET2 - 1 ||
    //         cz < -OFFSET2 || cz > OFFSET2 - 1) {
    //         throw std::out_of_range("cell coord out of packable range");
    //     }

    //     auto enc = [](int32_t c) -> uint64_t {
    //         uint64_t u = static_cast<uint64_t>(static_cast<int64_t>(c) + OFFSET2);
    //         return u & MASK2;
    //     };

    //     uint64_t ux = enc(cx);
    //     uint64_t uy = enc(cy);
    //     uint64_t uz = enc(cz);

    //     return (ux << (BITS2 * 2)) | (uy << BITS2) | uz;
    // }

    // static glm::vec3 unpack_key(uint64_t key) {
    //     uint64_t ux = (key >> (BITS2*2)) & MASK2;
    //     uint64_t uy = (key >> BITS2) & MASK2;
    //     uint64_t uz = key & MASK2;
    //     int32_t cx = (int)( (int)ux - OFFSET2 );
    //     int32_t cy = (int)( (int)uy - OFFSET2 );
    //     int32_t cz = (int)( (int)uz - OFFSET2 );

    //     return glm::ivec3(cx, cy, cz);
    // }
};