#pragma once

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include <unordered_set>
// #include <glm/gtc/matrix_transform.hpp>
// #include <glm/gtc/quaternion.hpp>

#include "../../vulkan_self/pass/instance/pass_instance.h"
#include "../aabb.h"
#include "../../vulkan_self/vulkan_buffer.h"
#include "../../vulkan_self/vulkan_fence.h"
#include "../../vulkan_self/vulkan_command_buffer.h"
#include "light_source.h"

#include "../../vulkan_self/logger/logger.h"


class ComputePassManager;
class Camera;
class VulkanCommandBuffer;
class Window;
class FrameResources;
struct FrameData;

class LightingSystem {
public:
    _XCLASS_NAME(LightingSystem);

    // struct alignas(16) LightingSystemUniform {
    //     uint32_t num_clusters;
    //     uint32_t max_lights_per_cluster;
    //     uint32_t pad0;
    //     uint32_t pad1;
    //     glm::mat4 view_matrix;
    // };

    struct alignas(16) LightingBuildUniform {
        uint32_t num_clusters;
        uint32_t max_lights_per_cluster;
        uint32_t num_light_sources;
        uint32_t pad0;

        glm::mat4 view_matrix;
    };

    struct alignas(16) ClusteredLightingUniform {
        glm::uvec4 cluster_grid;   
        // x = x clusters
        // y = y clusters
        // z = z clusters
        // w = max lights per cluster

        glm::vec4 screen_params;
        // x = screen width
        // y = screen height
        // z = near plane
        // w = far plane
    };

    explicit LightingSystem(VulkanEngine& engine, ComputePassManager& compute_pass_manager);

    void set_light_source(uint32_t slot_id, LightSource light_source);
    void update_light_sources(uint32_t frame_id);
    void update_clusters(const std::vector<AABB> &clusters, const glm::mat4& view_matrix);
    void update_light_indices_for_clusters(uint32_t frame_id, const Camera& camera);
    void set_cluster_aabbs(uint32_t frame_id, std::vector<AABB>& aabbs);
    bool sphere_intersects_aabb_view_space(const glm::vec3 &center_vs, float radius, const AABB &aabb) const noexcept;
    void compute_slice_distances_linear(float near_plane, float far_plane, unsigned z_slices, std::vector<float>& out);
    void compute_slice_distances_log(float near_plane, float far_plane, unsigned z_slices, std::vector<float>& out);
    void create_clusters_full(std::vector<AABB>& out_cluster_cells,
                              glm::uvec3 num_clusters,
                              float fov_y_radians, float aspect,
                              const std::vector<float>& slice_distances);
    void create_clusters(std::vector<AABB>& out_cluster_cells, glm::vec3 num_clusters,
                         float fov_y_radians, float aspect, float near_plane, float far_plane,
                         bool use_log_slices = false);
    void update_cluster_structure(uint32_t frame_id, const Window& window, const Camera& camera);
    void update_clustered_lighting_uniform(uint32_t frame_id, const Window& window, const Camera& camera);
    void update(uint32_t frame_id, const Window& window, const Camera& camera);

    void set_frame_resources(FrameResources& frame_resources) noexcept;

    uint32_t max_num_light_sources() const noexcept;
    uint32_t lights_in_clusters_size() const noexcept;
    uint32_t total_clusters_count() const noexcept;



private:
    size_t m_active_light_count = 0;
    size_t m_max_num_light_sources = 10000;
    size_t m_max_lights_per_cluster = 1500;
    glm::vec3 m_num_clusters{25, 25, 25};
    uint32_t m_total_clusters_count = m_num_clusters.x * m_num_clusters.y * m_num_clusters.z;
    uint32_t m_lights_in_clusters_size = m_total_clusters_count * m_max_lights_per_cluster;

    FrameResources* m_frame_resources = nullptr;

    VulkanEngine& m_engine;
    PassInstance m_build_cluster_light_lists_pass;
    VulkanCommandBuffer m_compute_command_buffer;
    VulkanFence m_compute_fence;

    std::vector<LightSource> m_light_sources;
    std::vector<std::vector<size_t>> m_lights_in_clusters;

    // VulkanBuffer m_light_source_ssbo;
    // VulkanBuffer m_lights_in_clusters_ssbo;
    // VulkanBuffer m_num_lights_in_clusters_ssbo;
    // VulkanBuffer m_cluster_aabbs_ssbo;
    // VulkanBuffer m_uniform_buffer;
    
    std::unordered_set<size_t> m_dirty_lights;
    std::vector<AABB> m_clusters;

    FrameData& get_frame_data(uint32_t frame_id);
};