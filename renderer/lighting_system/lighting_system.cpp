#include "lighting_system.h"


#include "light_source.h"
#include "../compute_pass_manager.h"
#include "../../vulkan_self/vulkan_engine.h"
#include "../../camera/camera.h"
#include "../../vulkan_self/window.h"
#include "../../vulkan_self/vulkan_command_buffer.h"
#include "../../math_utils.h"

LightingSystem::LightingSystem(VulkanEngine& engine, ComputePassManager& compute_pass_manager)
    :   m_engine(engine),
        m_build_cluster_light_lists_pass(compute_pass_manager.descriptor_pool(), compute_pass_manager.build_cluster_light_lists_cp),
    
    m_light_sources(m_max_num_light_sources, {glm::vec4(0.0f), glm::vec4(0.0f)}),
        m_lights_in_clusters(m_total_clusters_count),
        m_light_source_ssbo(VulkanBuffer::create_host_visible_storage_buffer(engine, sizeof(LightSource) * m_max_num_light_sources)),
        m_lights_in_clusters_ssbo(VulkanBuffer::create_host_visible_storage_buffer(engine, sizeof(unsigned int) * m_lights_in_clusters_size)),
        m_num_lights_in_clusters_ssbo(VulkanBuffer::create_host_visible_transfer_dst_storage_buffer(engine, sizeof(unsigned int) * m_total_clusters_count)),
        m_cluster_aabbs_ssbo(VulkanBuffer::create_host_visible_storage_buffer(engine, sizeof(AABB) * m_total_clusters_count)),
        m_uniform_buffer(VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(LightingSystemUniform))),
        m_compute_command_buffer(engine.device(), engine.compute_command_pool()),
        m_compute_fence(engine.device()) {
    LOG_METHOD();
    
    for (auto &v : m_lights_in_clusters)
        v.reserve(m_max_lights_per_cluster);
}

void LightingSystem::set_light_source(uint32_t slot_id, LightSource light_source) {
    LOG_METHOD();

    logger.check(slot_id < m_light_sources.size(), "Light source slot index was out of bounds");
    
    m_light_sources[slot_id] = light_source;
    m_dirty_lights.insert(slot_id);
}

void LightingSystem::update_light_sources() {
    LOG_METHOD();
    
    if (m_dirty_lights.empty())
        return;

    m_light_source_ssbo.upload(m_light_sources); // I'm not sure this is optimal

    m_dirty_lights.clear();
}

void LightingSystem::update_clusters(const std::vector<AABB> &clusters, const glm::mat4& view_matrix) {
    LOG_METHOD();
    
    for (int light_id = 0; light_id < m_light_sources.size(); light_id++) {
        LightSource& light_source = m_light_sources[light_id];
        glm::vec3 light_view_pos = glm::vec3(view_matrix * glm::vec4(light_source.position.x, light_source.position.y, light_source.position.z, 1.0f));
        float radius = light_source.position.w;

        for (int cluster_id = 0; cluster_id < clusters.size(); cluster_id++) {    
            if (light_id == 0)
                m_lights_in_clusters[cluster_id].clear();

            bool intersects = sphere_intersects_aabb_view_space(light_view_pos, radius, clusters[cluster_id]);

            if (intersects) {
                m_lights_in_clusters[cluster_id].push_back(light_id);
            }
        }
    }
}

void LightingSystem::update_light_indices_for_clusters(const Camera& camera) {
    LOG_METHOD();

    uint32_t x_count = math_utils::div_up_u32(m_total_clusters_count, 256u);
    uint32_t y_count = m_light_sources.size();

    LightingSystemUniform uniform_data{};
    uniform_data.num_clusters = m_total_clusters_count;
    uniform_data.max_lights_per_cluster = m_max_lights_per_cluster;
    uniform_data.view_matrix = camera.get_view_matrix();

    m_uniform_buffer.upload(&uniform_data, sizeof(LightingSystemUniform));

    m_build_cluster_light_lists_pass.set_uniform_buffer(0, m_uniform_buffer);
    m_build_cluster_light_lists_pass.set_storage_buffer(4, m_cluster_aabbs_ssbo);
    m_build_cluster_light_lists_pass.set_storage_buffer(5, m_light_source_ssbo);
    m_build_cluster_light_lists_pass.set_storage_buffer(6, m_num_lights_in_clusters_ssbo);
    m_build_cluster_light_lists_pass.set_storage_buffer(7, m_lights_in_clusters_ssbo);

    {
        auto compute_scope = m_compute_command_buffer.begin_scope();

        m_num_lights_in_clusters_ssbo.fill(m_compute_command_buffer, 0);
        m_num_lights_in_clusters_ssbo.transfer_write_to_compute_read_write_barrier(m_compute_command_buffer);
        
        m_build_cluster_light_lists_pass.bind(m_compute_command_buffer);

        m_compute_command_buffer.dispatch(x_count, y_count, 1);

        m_num_lights_in_clusters_ssbo.compute_write_to_fragment_read_barrier(m_compute_command_buffer);
        m_lights_in_clusters_ssbo.compute_write_to_fragment_read_barrier(m_compute_command_buffer);
    }

    m_compute_fence.reset();
    m_engine.compute_submit(m_compute_command_buffer, &m_compute_fence);
    m_compute_fence.wait();
}

void LightingSystem::set_cluster_aabbs(std::vector<AABB>& aabbs) {
    m_cluster_aabbs_ssbo.upload(aabbs);
}

bool LightingSystem::sphere_intersects_aabb_view_space(const glm::vec3 &center_vs, float radius, const AABB &aabb) const noexcept {
    float d2 = 0.0f;
    if (center_vs.x < aabb.min.x) { float d = aabb.min.x - center_vs.x; d2 += d*d; }
    else if (center_vs.x > aabb.max.x) { float d = center_vs.x - aabb.max.x; d2 += d*d; }
    if (center_vs.y < aabb.min.y) { float d = aabb.min.y - center_vs.y; d2 += d*d; }
    else if (center_vs.y > aabb.max.y) { float d = center_vs.y - aabb.max.y; d2 += d*d; }
    if (center_vs.z < aabb.min.z) { float d = aabb.min.z - center_vs.z; d2 += d*d; }
    else if (center_vs.z > aabb.max.z) { float d = center_vs.z - aabb.max.z; d2 += d*d; }
    return d2 <= radius * radius;
}

void LightingSystem::compute_slice_distances_linear(float near_plane, float far_plane, unsigned z_slices, std::vector<float>& out) {
    LOG_METHOD();

    logger.check(z_slices > 0, "z_slices must be greater than zero");
    logger.check(near_plane < far_plane, "near_plane must be less than far_plane");
    
    out.resize(z_slices + 1);

    for (unsigned i = 0; i <= z_slices; ++i) {
        float t = float(i) / float(z_slices);
        out[i] = glm::mix(near_plane, far_plane, t);
    }
}

void LightingSystem::compute_slice_distances_log(float near_plane, float far_plane, unsigned z_slices, std::vector<float>& out) {
    LOG_METHOD();
    
    logger.check(z_slices > 0, "z_slices must be greater than zero");
    logger.check(near_plane > 0.0f, "near_plane must be greater than zero for logarithmic slicing");
    logger.check(far_plane > 0.0f, "far_plane must be greater than zero for logarithmic slicing");
    logger.check(near_plane < far_plane, "near_plane must be less than far_plane");

    out.resize(z_slices + 1);

    float lnN = std::log(near_plane);
    float lnF = std::log(far_plane);

    for (unsigned i = 0; i <= z_slices; ++i) {
        float t = float(i) / float(z_slices);
        out[i] = std::exp(glm::mix(lnN, lnF, t));
    }
}

void LightingSystem::create_clusters_full(std::vector<AABB>& out_cluster_cells,
                                          glm::uvec3 num_clusters,
                                          float fov_y_radians, float aspect,
                                          const std::vector<float>& slice_distances) {
    LOG_METHOD();

    unsigned x_tiles = num_clusters.x;
    unsigned y_tiles = num_clusters.y;
    unsigned z_slices = num_clusters.z;

    logger.check(x_tiles > 0, "x cluster count must be greater than zero");
    logger.check(y_tiles > 0, "y cluster count must be greater than zero");
    logger.check(z_slices > 0, "z cluster count must be greater than zero");

    logger.check(
        slice_distances.size() == static_cast<size_t>(z_slices) + 1,
        "slice_distances size must be equal to z_slices + 1"
    );

    logger.check(aspect > 0.0f, "aspect ratio must be greater than zero");
    logger.check(fov_y_radians > 0.0f, "vertical field of view must be greater than zero");
    logger.check(fov_y_radians < glm::pi<float>(), "vertical field of view must be less than pi radians");

    out_cluster_cells.clear();
    out_cluster_cells.resize((size_t)x_tiles * y_tiles * z_slices);

    const float tan_half_fov_y = tanf(fov_y_radians * 0.5f);

    for (unsigned z = 0; z < z_slices; ++z) {
        float d0 = slice_distances[z];     // near bound for slice (positive)
        float d1 = slice_distances[z+1];   // far  bound
        // half heights/widths at both depths
        float hh0 = d0 * tan_half_fov_y;
        float hh1 = d1 * tan_half_fov_y;
        float hw0 = hh0 * aspect;
        float hw1 = hh1 * aspect;

        for (unsigned y = 0; y < y_tiles; ++y) {
            // normalized v coordinates (0..1). v=0 bottom, v=1 top
            float v0 = float(y) / float(y_tiles);
            float v1 = float(y + 1) / float(y_tiles);

            for (unsigned x = 0; x < x_tiles; ++x) {
                float u0 = float(x) / float(x_tiles);
                float u1 = float(x + 1) / float(x_tiles);

                // compute 8 corners of frustum cell in view space (z = -d)
                glm::vec3 corners[8];

                // near plane corners (d0)
                float xs0 = (u0 * 2.0f - 1.0f) * hw0;
                float xs1 = (u1 * 2.0f - 1.0f) * hw0;
                float ys0 = (v0 * 2.0f - 1.0f) * hh0;
                float ys1 = (v1 * 2.0f - 1.0f) * hh0;
                corners[0] = glm::vec3(xs0, ys0, -d0);
                corners[1] = glm::vec3(xs1, ys0, -d0);
                corners[2] = glm::vec3(xs1, ys1, -d0);
                corners[3] = glm::vec3(xs0, ys1, -d0);

                // far plane corners (d1)
                xs0 = (u0 * 2.0f - 1.0f) * hw1;
                xs1 = (u1 * 2.0f - 1.0f) * hw1;
                ys0 = (v0 * 2.0f - 1.0f) * hh1;
                ys1 = (v1 * 2.0f - 1.0f) * hh1;
                corners[4] = glm::vec3(xs0, ys0, -d1);
                corners[5] = glm::vec3(xs1, ys0, -d1);
                corners[6] = glm::vec3(xs1, ys1, -d1);
                corners[7] = glm::vec3(xs0, ys1, -d1);

                // create AABB from these 8 points
                glm::vec3 bmin( std::numeric_limits<float>::infinity() );
                glm::vec3 bmax( -std::numeric_limits<float>::infinity() );
                for (int i = 0; i < 8; ++i) {
                    bmin = glm::min(bmin, corners[i]);
                    bmax = glm::max(bmax, corners[i]);
                }

                // small epsilon padding (optional) to avoid numerical misses:
                const float eps = 1e-4f * (d0 + d1) * 0.5f; // scale with depth a bit
                bmin -= glm::vec3(eps);
                bmax += glm::vec3(eps);

                size_t idx = size_t(x) + size_t(y) * x_tiles + size_t(z) * x_tiles * y_tiles;
                out_cluster_cells[idx].min = glm::vec4(bmin.x, bmin.y, bmin.z, 1.0f);
                out_cluster_cells[idx].max = glm::vec4(bmax.x, bmax.y, bmax.z, 1.0f);
            }
        }
    }
}

void LightingSystem::create_clusters(std::vector<AABB>& out_cluster_cells, glm::vec3 num_clusters,
                                     float fov_y_radians, float aspect, float near_plane, float far_plane,
                                     bool use_log_slices) {
    use_log_slices = true;
    // convert glm::vec3 -> glm::uvec3
    glm::uvec3 tiles = glm::uvec3((unsigned)num_clusters.x, (unsigned)num_clusters.y, (unsigned)num_clusters.z);

    std::vector<float> sliceDistances;
    if (use_log_slices) compute_slice_distances_log(near_plane, far_plane, tiles.z, sliceDistances);
    else                compute_slice_distances_linear(near_plane, far_plane, tiles.z, sliceDistances);

    create_clusters_full(out_cluster_cells, tiles, fov_y_radians, aspect, sliceDistances);
}

void LightingSystem::update_cluster_structure(const Window& window, const Camera& camera) {
    bool need_recreate_clusters = false;

    if (m_cluster_fov != glm::radians(camera.fov)) {
        m_cluster_fov = glm::radians(camera.fov);
        need_recreate_clusters = true;
    }
    // float aspect = window.get_fbuffer_aspect_ratio();

    float aspect = float(window.width()) / float(window.height());
    if (m_cluster_aspect != aspect) {
        m_cluster_aspect = aspect;
        need_recreate_clusters = true;
    }
    if (m_cluster_near != camera.near_plane) {
        m_cluster_near = camera.near_plane;
        need_recreate_clusters = true;
    }
    if (m_cluster_far != camera.far_plane) {
        m_cluster_far = camera.far_plane;
        need_recreate_clusters = true;
    }
    
    if (need_recreate_clusters) {
        create_clusters(m_clusters, m_num_clusters, m_cluster_fov, m_cluster_aspect, m_cluster_near, m_cluster_far);
        set_cluster_aabbs(m_clusters);
    }
}

void LightingSystem::update(const Window& window, const Camera& camera) {
    update_cluster_structure(window, camera);
    update_light_sources();
    update_light_indices_for_clusters(camera);
}