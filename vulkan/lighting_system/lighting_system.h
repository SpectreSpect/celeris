#pragma once
#include <vector>
#include <unordered_set>
// #include "../camera.h"// #include "../math_utils.h"
#include "light_source.h"
#include "../video_buffer.h"
#include "../../camera.h"
#include "../shader_module.h"
#include "../../vulkan_engine.h"
#include "../compute_pipeline.h"
#include "../fence.h"
#include "../command_pool.h"
#include "../command_buffer.h"
#include "../../path_utils.h"
// #include "window.h"

class LightingSystem {
public:
    // struct LightingSystemUniform{
    //     uint32_t num_clusters;
    //     uint32_t max_lights_per_cluster;
    //     glm::mat4 view_matrix;
    // };

    struct alignas(16) LightingSystemUniform {
        uint32_t num_clusters;
        uint32_t max_lights_per_cluster;
        uint32_t pad0;
        uint32_t pad1;
        glm::mat4 view_matrix;
    };

    std::vector<LightSource> light_sources;
    std::unordered_set<size_t> dirty_lights;
    VideoBuffer cluster_aabbs_ssbo;
    VideoBuffer light_source_ssbo;
    VideoBuffer num_lights_in_clusters_ssbo;
    VideoBuffer lights_in_clusters_ssbo;
    std::vector<std::vector<size_t>> lights_in_clusters;
    std::vector<AABB> clusters;
    
    size_t max_num_light_sources = 10000;
    size_t max_lights_per_cluster = 1500;
    glm::vec3 num_clusters{25, 25, 25};

    VulkanShaderModule* light_indices_for_clusters_program = nullptr;
    
    float cluster_fov = 0.0f;
    float cluster_aspect = 0.0f;
    float cluster_near = 0.0f;
    float cluster_far = 0.0f;

    LightingSystem() = default;
    void init(VulkanEngine& engine);
    // void update_lights_in_clusters();
    void set_light_source(size_t id, LightSource light_source);
    void update_light_sources();
    bool sphereIntersectsAABB_ViewSpace(const glm::vec3 &centerVS, float radius, const AABB &aabb);
    void update_clusters(const std::vector<AABB> &clusters, const glm::mat4& view_matrix);
    void update_light_indices_for_clusters(const Camera& camera);
    void set_cluster_aabbs(std::vector<AABB>& aabbs);
    void computeSliceDistancesLinear(float nearPlane, float farPlane, unsigned zSlices, std::vector<float>& out);
    void computeSliceDistancesLog(float nearPlane, float farPlane, unsigned zSlices, std::vector<float>& out);
    void create_clusters(std::vector<AABB>& out_cluster_cells, glm::vec3 num_clusters,
                         float fovY_radians, float aspect, float nearPlane, float farPlane,
                         bool useLogSlices = false);
    static void create_clusters_full(std::vector<AABB>& out_cluster_cells, glm::uvec3 numClusters,
                                     float fovY_radians, float aspect, const std::vector<float>& sliceDistances);


    // // void bind_buffers();
    void update_cluster_structure(const Camera& camera);
    void update(const Camera& camera);

    CommandPool command_pool;
    CommandBuffer command_buffer;
    VideoBuffer lighting_system_uniform_buffer;
private:
    VulkanEngine* engine = nullptr;
    ComputePipeline pipeline;
    DescriptorSetBundle descriptor_set_bundle;
    Fence fence;
    uint32_t cubemap_face_size;
    uint32_t compute_queue_family_id;
    VkQueue compute_queue;
};