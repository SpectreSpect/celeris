#pragma once
#include <vector>
#include <unordered_set>
#include "ssbo.h"
#include "light_source.h"
#include "compute_program.h"
#include "camera.h"
#include "math_utils.h"
#include "window.h"
#include "shader_manager.h"

class LightingSystem {
public:
    std::vector<LightSource> light_sources;
    std::unordered_set<size_t> dirty_lights;
    SSBO cluster_aabbs_ssbo;
    SSBO light_source_ssbo;
    SSBO num_lights_in_clusters_ssbo;
    SSBO lights_in_clusters_ssbo;
    std::vector<std::vector<size_t>> lights_in_clusters;
    std::vector<AABB> clusters;
    
    size_t max_num_light_sources = 10000;
    size_t max_lights_per_cluster = 1500;
    glm::vec3 num_clusters{25, 25, 25};

    ComputeProgram* light_indices_for_clusters_program;
    
    float cluster_fov = 0.0f;
    float cluster_aspect = 0.0f;
    float cluster_near = 0.0f;
    float cluster_far = 0.0f;

    LightingSystem() = default;
    void init(ShaderManager& shader_manager);
    void update_lights_in_clusters();
    void set_light_source(size_t id, LightSource light_source);
    void update_light_sources();
    bool sphereIntersectsAABB_ViewSpace(const glm::vec3 &centerVS, float radius, const AABB &aabb);
    void update_clusters(const std::vector<AABB> &clusters, const glm::mat4& view_matrix);
    void update_light_indices_for_clusters(ComputeProgram& light_indices_for_clusters_program, const Camera& camera);
    void set_cluster_aabbs(std::vector<AABB>& aabbs);
    void computeSliceDistancesLinear(float nearPlane, float farPlane, unsigned zSlices, std::vector<float>& out);
    void computeSliceDistancesLog(float nearPlane, float farPlane, unsigned zSlices, std::vector<float>& out);
    void create_clusters(std::vector<AABB>& out_cluster_cells, glm::vec3 num_clusters,
                         float fovY_radians, float aspect, float nearPlane, float farPlane,
                         bool useLogSlices = false);
    static void create_clusters_full(std::vector<AABB>& out_cluster_cells, glm::uvec3 numClusters,
                                     float fovY_radians, float aspect, const std::vector<float>& sliceDistances);


    void bind_buffers();
    void update_cluster_structure(const Camera& camera, const Window& window);
    void update(const Camera& camera, const Window& window);
};