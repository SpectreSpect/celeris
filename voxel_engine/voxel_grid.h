#pragma once
#include "chunk.h"
#include <unordered_map>
#include <iostream>
#include <thread>
#include <set>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <unordered_set>
#include <utility>
#include "../window.h"
#include "voxel_editor.h"
#include <algorithm> 
#include "../grid_3d.h"
#include "../gridable.h"
#include "../math_utils.h"

struct MeshJob {
    uint64_t key;
    glm::ivec3 cpos; // chunk coords
    glm::ivec3 chunk_size;
    uint32_t revision;
    uint32_t ticket;

    // std::shared_ptr<const std::vector<Voxel>> voxels;
    std::shared_ptr<const std::vector<Voxel>> self;
    std::array<std::shared_ptr<const std::vector<Voxel>>, 6> nb;
};

struct MeshResult {
    uint64_t key;
    MeshData mesh_data;
    uint32_t revision;
    uint32_t ticket;
    // std::vector<float> vertices;
    // std::vector<unsigned int> indices;
};

struct GenJob {
    uint64_t key;
    glm::ivec3 cpos;
    glm::ivec3 chunk_size;
};

struct GenResult {
    uint64_t key;
    glm::ivec3 cpos;
    std::shared_ptr<const std::vector<Voxel>> voxels;
};

// class VoxelGrid;

<<<<<<< HEAD
class VoxelGrid : public Drawable, public IGridable, public Transformable  {
=======
class VoxelGrid : public Grid3D, public Gridable, public Drawable {
>>>>>>> origin/cloud-to-mesh
public:
    // int chunk_render_distance = 8;
    glm::ivec3 chunk_render_size;
    glm::ivec3 chunk_size;
    float voxel_size;
    std::unordered_map<uint64_t, Chunk*> chunks;
    std::set<uint64_t> chunks_to_update;
    // bool placed = false;
    
    VoxelGrid(glm::ivec3 chunk_size, float voxel_size, glm::ivec3 chunk_render_size = {16, 6, 16});
    ~VoxelGrid();

    bool is_solid(glm::ivec3 pos) override;
    bool is_voxel_free(glm::ivec3 pos);

    // std::thread mesh_updating_thread;
    std::vector<std::thread> mesh_workers;
    std::atomic<bool> mesh_thread_running{false};

    std::mutex jobs_mx;
    std::condition_variable jobs_cv;
    std::deque<MeshJob> jobs;
    std::unordered_set<uint64_t> in_flight;

    template<class F>
    void edit_voxels(F&& apply_edits) {
        VoxelEditor voxel_editor = VoxelEditor(this);
        apply_edits(voxel_editor);

        voxel_editor.update_and_schedule();
    }
    template<class F>
    void edit_chunk(glm::ivec3 chunk_pos, Chunk* chunk, F&& apply_edits) {
        uint64_t key = math_utils::pack_key(chunk_pos.x, chunk_pos.y, chunk_pos.z);   

        chunk->edit_voxels([&](std::vector<Voxel>& voxels){
            apply_edits(voxels);
        });

        chunks[key] = chunk;
        chunks_to_update.insert(key);
        chunks_to_update.insert(math_utils::pack_key(chunk_pos.x-1, chunk_pos.y, chunk_pos.z)); // left
        chunks_to_update.insert(math_utils::pack_key(chunk_pos.x, chunk_pos.y, chunk_pos.z-1)); // back
        chunks_to_update.insert(math_utils::pack_key(chunk_pos.x+1, chunk_pos.y, chunk_pos.z)); // right
        chunks_to_update.insert(math_utils::pack_key(chunk_pos.x, chunk_pos.y, chunk_pos.z+1)); // front
        chunks_to_update.insert(math_utils::pack_key(chunk_pos.x, chunk_pos.y+1, chunk_pos.z)); // top
        chunks_to_update.insert(math_utils::pack_key(chunk_pos.x, chunk_pos.y-1, chunk_pos.z)); // bottom
    }

    static glm::ivec3 get_chunk_pos(glm::ivec3 vpos, glm::ivec3 chunk_size) {
        return {
            math_utils::floor_div(vpos.x, chunk_size.x),
            math_utils::floor_div(vpos.y, chunk_size.y),
            math_utils::floor_div(vpos.z, chunk_size.z),
        };
    }

    static inline uint32_t hash32(uint32_t x) {
        x += 0x9e3779b9u;
        x = (x ^ (x >> 16)) * 0x85ebca6bu;
        x = (x ^ (x >> 13)) * 0xc2b2ae35u;
        x ^= (x >> 16);
        return x;
    }

    static inline uint32_t rand2i(int32_t x, int32_t y) {
        // Mix coords into a 32-bit seed first (works fine with negatives)
        uint32_t ux = (uint32_t)x;
        uint32_t uy = (uint32_t)y;
        uint32_t seed = ux * 0x8da6b343u ^ uy * 0xd8163841u; // big odd constants
        return hash32(seed);
    }

    Voxel get_voxel(glm::ivec3 pos);

    bool enqueue_mesh_job(uint64_t key, glm::ivec3 cpos, Chunk* chunk);
    void mesh_worker_loop();
    void drain_mesh_results();

    std::mutex results_mx;
    std::deque<MeshResult> results;

    
    std::vector<std::thread> gen_workers;
    std::atomic<bool> gen_thread_running{false};
    std::mutex gen_jobs_mx;
    std::condition_variable gen_jobs_cv;
    std::deque<GenJob> gen_jobs;
    std::mutex gen_results_mx;
    std::deque<GenResult> gen_results;

    std::shared_ptr<std::vector<Voxel>> generate_chunk(glm::ivec3 chunk_pos, glm::ivec3 chunk_size);

    void gen_worker_loop();
    void enqueue_gen_job(uint64_t key, glm::ivec3 cpos, glm::ivec3 chunk_size);
    void drain_gen_results();

    virtual void set_voxels(const std::vector<Voxel>& voxels, const std::vector<glm::ivec3>& positions) override;
    virtual void set_voxel(const Voxel& voxel, glm::ivec3 position) override;
    virtual Voxel get_voxel(glm::ivec3 position) const override;

    void update(Window* window, Camera* camera);
    void draw(RenderState state) override;
};