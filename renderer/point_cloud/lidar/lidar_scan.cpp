#include "lidar_scan.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>

#include <glm/gtc/quaternion.hpp>

#include "../point_instance.h"
#include "../../../managers/manager_bundle.h"
#include "../point_cloud_preprocessor.h"

namespace {
constexpr size_t imu_floats_per_scan_point = 14;

glm::quat normalized_quat_or_identity(float qx, float qy, float qz, float qw) {
    glm::quat q(qw, qx, qy, qz);
    const float len = glm::length(q);
    if (!std::isfinite(len) || len <= 1e-6f)
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    return glm::normalize(q);
}

glm::mat3 sample_orientation_matrix(const LidarScan::FrameData& frame, size_t sample_id) {
    if (frame.sample_orientations.size() == frame.samples.size()) {
        glm::quat q = frame.sample_orientations[sample_id];
        const float len = glm::length(q);
        if (std::isfinite(len) && len > 1e-6f)
            return glm::mat3_cast(glm::normalize(q));
    }

    return glm::mat3(1.0f);
}
}

LidarScan::LidarScan(
    ManagerBundle& manager_bundle, 
    PointCloudPreprocessor& point_cloud_preprocessor, 
    const std::filesystem::path& path) 
    :   m_point_cloud(load_from_file(manager_bundle, path)),
        m_normal_buffer(
            VulkanBuffer::create_host_visible_storage_buffer(
                manager_bundle.engine(), 
                m_point_cloud.point_count() * sizeof(glm::vec4)
            )
        ) 
{
    point_cloud_preprocessor.remove_points_near_origin(
        m_point_cloud.instance_buffer(),
        m_point_cloud.point_count()
    );
    point_cloud_preprocessor.get_normals_from_webots_lidar_point_cloud(
        m_point_cloud.instance_buffer(), 
        m_normal_buffer, 
        m_point_cloud.point_count(),
        16
    );
    add_child(m_point_cloud);
}

LidarScan::LidarScan(
    ManagerBundle& manager_bundle, 
    PointCloudPreprocessor& point_cloud_preprocessor, 
    FrameData&& frame)
    :   m_point_cloud(load_from_frame(manager_bundle, std::move(frame))),
        m_normal_buffer(
            VulkanBuffer::create_host_visible_storage_buffer(
                manager_bundle.engine(), 
                m_point_cloud.point_count() * sizeof(glm::vec4)
            )
        ) 
{
    point_cloud_preprocessor.remove_points_near_origin(
        m_point_cloud.instance_buffer(),
        m_point_cloud.point_count()
    );
    point_cloud_preprocessor.get_normals_from_webots_lidar_point_cloud(
        m_point_cloud.instance_buffer(), 
        m_normal_buffer, 
        m_point_cloud.point_count(),
        frame.ring_count
    );
    add_child(m_point_cloud);
}

void LidarScan::set_timestamp_ns(uint64_t timestamp_ns) {
    m_timestamp_ns = timestamp_ns;
}

uint64_t LidarScan::timestamp_ns() const noexcept {
    return m_timestamp_ns;
}

glm::vec3 LidarScan::linear_acceleration() const noexcept {
    return m_linear_acceleration;
}

glm::vec3 LidarScan::angular_velocity() const noexcept {
    return m_angular_velocity;
}

glm::quat LidarScan::orientation() const noexcept {
    return m_orientation;
}

LidarScan::FrameData LidarScan::read_frame_from_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Failed to open: " + path.string());

    FrameData frame;
    uint32_t count = 0;

    in.read(reinterpret_cast<char*>(&frame.timestamp_ns), sizeof(uint64_t));
    in.read(reinterpret_cast<char*>(&count), sizeof(uint32_t));
    if (!in) throw std::runtime_error("Bad header in: " + path.string());

    const size_t bpp = imu_floats_per_scan_point * sizeof(float);

    std::vector<uint8_t> buf(size_t(count) * bpp);
    in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    if (!in) throw std::runtime_error("Unexpected EOF in: " + path.string());

    frame.samples.resize(count);
    frame.sample_orientations.resize(count);
    frame.sample_linear_accelerations.resize(count);
    frame.sample_angular_velocities.resize(count);

    const uint8_t* p = buf.data();

    for (uint32_t i = 0; i < count; ++i) {
        float x, y, z;
        float time;
        float qx, qy, qz, qw;
        float ax, ay, az;
        float wx, wy, wz;

        std::memcpy(&x,     p, 4); p += 4;
        std::memcpy(&y,     p, 4); p += 4;
        std::memcpy(&z,     p, 4); p += 4;
        std::memcpy(&time,  p, 4); p += 4;
        std::memcpy(&ax, p, 4); p += 4;
        std::memcpy(&ay, p, 4); p += 4;
        std::memcpy(&az, p, 4); p += 4;
        std::memcpy(&wx, p, 4); p += 4;
        std::memcpy(&wy, p, 4); p += 4;
        std::memcpy(&wz, p, 4); p += 4;
        std::memcpy(&qx, p, 4); p += 4;
        std::memcpy(&qy, p, 4); p += 4;
        std::memcpy(&qz, p, 4); p += 4;
        std::memcpy(&qw, p, 4); p += 4;

        frame.sample_linear_accelerations[i] = glm::vec3(ax, ay, az);
        frame.sample_angular_velocities[i] = glm::vec3(wx, wy, wz);
        frame.sample_orientations[i] = normalized_quat_or_identity(qx, qy, qz, qw);

        frame.samples[i].p_local = glm::vec3(x, y, z);
        frame.samples[i].time = time;
        frame.samples[i].valid = std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }

    frame.ring_count = 16;
    build_points_for_frame(frame);

    return frame;
}

PointCloud LidarScan::load_from_file(ManagerBundle& manager_bundle, const std::filesystem::path& path) {
    return load_from_frame(manager_bundle, read_frame_from_file(path));
}

PointCloud LidarScan::load_from_frame(ManagerBundle& manager_bundle, FrameData&& frame) {
    m_timestamp_ns = frame.timestamp_ns;
    m_linear_acceleration = glm::vec3(0.0f);
    m_angular_velocity = glm::vec3(0.0f);
    m_orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    if (!frame.sample_linear_accelerations.empty()) {
        glm::vec3 sum(0.0f);
        uint32_t valid_count = 0;

        for (const glm::vec3& acceleration : frame.sample_linear_accelerations) {
            if (!std::isfinite(acceleration.x) ||
                !std::isfinite(acceleration.y) ||
                !std::isfinite(acceleration.z)) {
                continue;
            }

            sum += acceleration;
            valid_count++;
        }

        if (valid_count > 0u) {
            m_linear_acceleration = sum / static_cast<float>(valid_count);
        }
    }

    if (!frame.sample_angular_velocities.empty()) {
        glm::vec3 sum(0.0f);
        uint32_t valid_count = 0;
        for (const glm::vec3& velocity : frame.sample_angular_velocities) {
            if (std::isfinite(velocity.x) &&
                std::isfinite(velocity.y) &&
                std::isfinite(velocity.z)) {
                sum += velocity;
                valid_count++;
            }
        }
        if (valid_count > 0u) {
            m_angular_velocity = sum / static_cast<float>(valid_count);
        }
    }

    if (!frame.sample_orientations.empty()) {
        size_t reference_index = 0;
        for (size_t i = 1; i < frame.samples.size(); ++i) {
            if (frame.samples[i].time < frame.samples[reference_index].time) {
                reference_index = i;
            }
        }
        m_orientation = glm::normalize(frame.sample_orientations[reference_index]);
    }

    if (frame.points.empty()) {
        throw std::runtime_error("LidarScan frame had no points");
    }

    m_points = std::move(frame.points);

    return PointCloud(manager_bundle, m_points);
}

void LidarScan::build_points_for_frame(FrameData& frame) {
    const uint32_t count = static_cast<uint32_t>(frame.samples.size());
    if (count == 0) {
        frame.points.clear();
        return;
    }

    float min_time = std::numeric_limits<float>::infinity();
    size_t ref_idx = 0;

    for (uint32_t i = 0; i < count; ++i) {
        if (frame.samples[i].time < min_time) {
            min_time = frame.samples[i].time;
            ref_idx = i;
        }
    }

    frame.points.clear();
    frame.points.resize(count);

    const float INF = std::numeric_limits<float>::infinity();

    const glm::mat3 R_wb_ref = sample_orientation_matrix(frame, ref_idx);

    for (uint32_t i = 0; i < count; ++i) {
        const TimedPointSample& s = frame.samples[i];

        if (!s.valid) {
            frame.points[i].position = glm::vec4(INF, INF, INF, 1.0f);
            frame.points[i].color = glm::vec4(1, 1, 1, 1);
            continue;
        }

        const glm::mat3 R_wb = sample_orientation_matrix(frame, i);
        const glm::vec3 p_ref =
            glm::transpose(R_wb_ref) * (R_wb * s.p_local);

        frame.points[i].position = glm::vec4(p_ref, 1.0f);
        frame.points[i].color = glm::vec4(1, 1, 1, 1);
    }
}

// glm::vec3 LidarScan::ros_pos_to_engine(const glm::vec3& p_ros)
// {
//     return glm::vec3(-p_ros.x, p_ros.z, p_ros.y);
// }

// glm::mat3 LidarScan::ros_rotation_to_engine(const glm::mat3& rotation_ros)
// {
//     glm::mat3 basis(1.0f);
//     basis[0] = glm::vec3(-1.0f, 0.0f, 0.0f);
//     basis[1] = glm::vec3(0.0f, 0.0f, 1.0f);
//     basis[2] = glm::vec3(0.0f, 1.0f, 0.0f);
//     return basis * rotation_ros * glm::transpose(basis);
// }
    
PointCloud& LidarScan::point_cloud() {
    return m_point_cloud;
}

VulkanBuffer& LidarScan::normal_buffer() {
    return m_normal_buffer;
}

// std::vector<glm::vec4> LidarScan::calculate_normals(std::vector<PointInstance> points) {
//     std::vector<glm::vec4> normals;

//     get_normals(m_points, normals);
//     remove_invalid_points_and_normals(m_points, normals);
//     drop_out_points_and_normals(m_points, normals, 10000);
//     remove_points_near_origin(m_points, normals, 3);

//     return normals;
// }

// void LidarScan::remove_points_near_origin(std::vector<PointInstance>& points,
//                                            std::vector<glm::vec4>& normals,
//                                            float min_distance)
// {
//     if (points.size() != normals.size()) {
//         std::cout << "remove_points_near_origin: points.size() != normals.size()\n";
//         return;
//     }

//     float min_dist_sq = min_distance * min_distance;

//     std::vector<PointInstance> filtered_points;
//     std::vector<glm::vec4> filtered_normals;

//     filtered_points.reserve(points.size());
//     filtered_normals.reserve(normals.size());

//     for (size_t i = 0; i < points.size(); i++) {
//         const PointInstance& p = points[i];
//         const glm::vec4& n = normals[i];

//         glm::vec3 pos = glm::vec3(p.pos);
//         float dist_sq = glm::dot(pos, pos);

//         // remove points that are too close to the lidar origin
//         // if (dist_sq < min_dist_sq || p.pos.y < 1.0)
//         if (dist_sq < min_dist_sq)
//             continue;

//         filtered_points.push_back(p);
//         filtered_normals.push_back(n);
//     }

//     points = std::move(filtered_points);
//     normals = std::move(filtered_normals);
// }

// void LidarScan::drop_out_points_and_normals(std::vector<PointInstance>& points,
//                                              std::vector<glm::vec4>& normals,
//                                              size_t target_size)
// {
//     if (points.size() != normals.size()) {
//         std::cout << "drop_out_points_and_normals: points.size() != normals.size()\n";
//         return;
//     }

//     size_t n = points.size();

//     if (target_size >= n) {
//         return; // nothing to drop
//     }

//     if (target_size == 0) {
//         points.clear();
//         normals.clear();
//         return;
//     }

//     // Create indices [0, 1, 2, ..., n-1]
//     std::vector<size_t> indices(n);
//     std::iota(indices.begin(), indices.end(), 0);

//     // Shuffle indices randomly
//     static std::random_device rd;
//     static std::mt19937 rng(rd());
//     std::shuffle(indices.begin(), indices.end(), rng);

//     // Keep only the first target_size indices
//     indices.resize(target_size);

//     // Optional: sort so the remaining points keep their original relative order
//     std::sort(indices.begin(), indices.end());

//     std::vector<PointInstance> new_points;
//     std::vector<glm::vec4> new_normals;

//     new_points.reserve(target_size);
//     new_normals.reserve(target_size);

//     for (size_t idx : indices) {
//         new_points.push_back(points[idx]);
//         new_normals.push_back(normals[idx]);
//     }

//     points = std::move(new_points);
//     normals = std::move(new_normals);
// }


// void LidarScan::keep_only_upward_facing_points_and_normals(
//     std::vector<PointInstance>& points,
//     std::vector<glm::vec4>& normals,
//     float up_dot_threshold)
// {
//     if (points.size() != normals.size()) {
//         std::cout << "keep_only_upward_facing_points_and_normals: points.size() != normals.size()\n";
//         return;
//     }

//     std::vector<PointInstance> kept_points;
//     std::vector<glm::vec4> kept_normals;

//     kept_points.reserve(points.size());
//     kept_normals.reserve(normals.size());

//     const glm::vec3 up(0.0f, 1.0f, 0.0f);

//     for (size_t i = 0; i < points.size(); ++i) {
//         const PointInstance& p = points[i];
//         const glm::vec3 n = glm::vec3(normals[i]);

//         if (!is_point_valid(p) || glm::dot(n, n) < 1e-12f) {
//             continue;
//         }

//         float up_dot = glm::dot(glm::normalize(n), up);

//         if (up_dot >= up_dot_threshold) {
//             kept_points.push_back(p);
//             kept_normals.push_back(normals[i]);
//         }
//     }

//     points = std::move(kept_points);
//     normals = std::move(kept_normals);
// }



// void LidarScan::remove_ground_points_and_normals(
//     std::vector<PointInstance>& points,
//     std::vector<glm::vec4>& normals,
//     float up_dot_threshold,
//     float max_ground_height)
// {
//     if (points.size() != normals.size()) {
//         std::cout << "remove_ground_points_and_normals: points.size() != normals.size()\n";
//         return;
//     }

//     std::vector<PointInstance> filtered_points;
//     std::vector<glm::vec4> filtered_normals;

//     filtered_points.reserve(points.size());
//     filtered_normals.reserve(normals.size());

//     const glm::vec3 up(0.0f, 1.0f, 0.0f);

//     for (size_t i = 0; i < points.size(); ++i) {
//         const PointInstance& p = points[i];
//         const glm::vec3 n = glm::vec3(normals[i]);

//         // Keep invalid points/normals handling simple
//         if (!is_point_valid(p) || glm::dot(n, n) < 1e-12f) {
//             filtered_points.push_back(p);
//             filtered_normals.push_back(normals[i]);
//             continue;
//         }

//         glm::vec3 nn = glm::normalize(n);
//         float up_dot = glm::dot(nn, up);

//         bool looks_like_ground =
//             (up_dot >= up_dot_threshold) &&
//             (p.pos.y <= max_ground_height);

//         if (!looks_like_ground) {
//             filtered_points.push_back(p);
//             filtered_normals.push_back(normals[i]);
//         }
//     }

//     points = std::move(filtered_points);
//     normals = std::move(filtered_normals);
// }


// void LidarScan::get_normals(const std::vector<PointInstance>& points, std::vector<glm::vec4>& normals)
// {
//     normals.clear();
//     normals.resize(points.size(), glm::vec4(0.0f));

//     if (points.empty())
//         return;

//     const int rings_count = 16;
//     const int cloud_size = static_cast<int>(points.size());

//     if (cloud_size < rings_count)
//         return;

//     const int ring_width = cloud_size / rings_count;
//     if (ring_width < 2)
//         return;

//     const float rel_thresh = 1.0f;

//     auto accumulate_triangle_normal = [&](int ia, int ib, int ic)
//     {
//         const PointInstance& a = points[ia];
//         const PointInstance& b = points[ib];
//         const PointInstance& c = points[ic];

//         // Extra safety: don't accumulate into invalid points
//         if (!is_point_valid(a) || !is_point_valid(b) || !is_point_valid(c))
//             return;

//         glm::vec3 n = triangle_normal(a, b, c);

//         if (glm::dot(n, n) < 1e-12f)
//             return;

//         // Keep normals consistently oriented upward
//         if (glm::dot(n, glm::vec3(0.0f, 1.0f, 0.0f)) < 0.0f)
//             n = -n;

//         glm::vec4 n4(n, 0.0f);

//         normals[ia] += n4;
//         normals[ib] += n4;
//         normals[ic] += n4;
//     };

//     for (int y = 0; y < rings_count - 1; y++) {
//         for (int x = 0; x < ring_width - 1; x++) {
//             int id1 = xy_id(x,     y,     ring_width, cloud_size);
//             int id2 = xy_id(x,     y + 1, ring_width, cloud_size);
//             int id3 = xy_id(x + 1, y + 1, ring_width, cloud_size);
//             int id5 = xy_id(x + 1, y,     ring_width, cloud_size);

//             const PointInstance& p0 = points[id1]; // lower-left
//             const PointInstance& p1 = points[id2]; // upper-left
//             const PointInstance& p2 = points[id3]; // upper-right
//             const PointInstance& p3 = points[id5]; // lower-right

//             bool tri1_ok = false;
//             bool tri2_ok = false;

//             if (is_point_valid(p0) && is_point_valid(p1) && is_point_valid(p2)) {
//                 if (is_same_object(p0, p1, rel_thresh) &&
//                     is_same_object(p1, p2, 1.5f, false))
//                 {
//                     tri1_ok = true;
//                 }
//             }

//             if (is_point_valid(p2) && is_point_valid(p3) && is_point_valid(p0)) {
//                 if (is_same_object(p3, p2, rel_thresh) &&
//                     is_same_object(p3, p0, 1.5f, false))
//                 {
//                     tri2_ok = true;
//                 }
//             }

//             if (tri1_ok) {
//                 accumulate_triangle_normal(id3, id1, id2);
//             }

//             if (tri2_ok) {
//                 accumulate_triangle_normal(id1, id3, id5);
//             }
//         }
//     }

//     // Normalize only valid points that actually accumulated something.
//     // Invalid points remain degenerate: (0,0,0,0).
//     for (size_t i = 0; i < normals.size(); i++) {
//         if (!is_point_valid(points[i])) {
//             normals[i] = glm::vec4(0.0f);
//             continue;
//         }

//         glm::vec3 n = glm::vec3(normals[i]);
//         float len2 = glm::dot(n, n);

//         if (len2 < 1e-12f) {
//             normals[i] = glm::vec4(0.0f);
//         } else {
//             normals[i] = glm::vec4(glm::normalize(n), 0.0f);
//         }
//     }
// }

// void LidarScan::remove_invalid_points_and_normals(std::vector<PointInstance>& points,
//                                                    std::vector<glm::vec4>& normals)
// {
//     if (points.size() != normals.size()) {
//         std::cout << "remove_invalid_points_and_normals: points.size() != normals.size()\n";
//         return;
//     }

//     std::vector<PointInstance> filtered_points;
//     std::vector<glm::vec4> filtered_normals;

//     filtered_points.reserve(points.size());
//     filtered_normals.reserve(normals.size());

//     for (size_t i = 0; i < points.size(); i++) {
//         const PointInstance& p = points[i];
//         const glm::vec4& n4 = normals[i];
//         glm::vec3 n = glm::vec3(n4);

//         bool point_valid = is_point_valid(p);
//         bool normal_valid = glm::dot(n, n) > 1e-12f;

//         if (!point_valid || !normal_valid)
//             continue;

//         filtered_points.push_back(p);
//         filtered_normals.push_back(glm::vec4(glm::normalize(n), 0.0f));
//     }

//     points = std::move(filtered_points);
//     normals = std::move(filtered_normals);
// }

// bool LidarScan::is_point_valid(const PointInstance &p) {
//     return std::isfinite(p.pos.x) && std::isfinite(p.pos.y) && std::isfinite(p.pos.z);
// }

// glm::vec3 LidarScan::triangle_normal(const PointInstance& a, const PointInstance& b, const PointInstance& c) {
//     glm::vec3 av = {a.pos.x, a.pos.y, a.pos.z};
//     glm::vec3 bv = {b.pos.x, b.pos.y, b.pos.z};
//     glm::vec3 cv = {c.pos.x, c.pos.y, c.pos.z};

//     glm::vec3 u = bv - av;
//     glm::vec3 v = cv - av;
//     glm::vec3 n = glm::cross(u, v);           // unnormalized normal (also proportional to triangle area)
//     return glm::normalize(n);       // returns {0,0,0} if degenerate
// }

// int LidarScan::xy_id(int x, int y, int ring_width, int cloud_size) {
//     int idx = x + y * ring_width;

//     if (idx < 0 || idx >= cloud_size) {
//         throw "Bad index";
//         return -1;
//     }

//     return idx;
// }

// float LidarScan::radial_distance(const PointInstance &p) {
//     return std::hypot(static_cast<double>(p.pos.x), static_cast<double>(p.pos.y), static_cast<double>(p.pos.z));
// }

// bool LidarScan::is_same_object(
//     const PointInstance &p0,
//     const PointInstance &p1,
//     float rel_thresh,
//     bool more_permissive_with_distance,
//     float abs_thresh)
// {
//     if (!is_point_valid(p0) || !is_point_valid(p1))
//         return false;

//     float r0 = radial_distance(p0);
//     float r1 = radial_distance(p1);

//     if (!std::isfinite(r0) || !std::isfinite(r1))
//         return false;

//     float allowed = 0;
//     float dr = std::fabs(r0 - r1);


//     if (more_permissive_with_distance) {
//         // float thresh = std::max(0.2f - p0.pos.y, 0.0f);
//         // allowed = std::max(thresh * std::min(r0, r1), abs_thresh);
//         float permission_factor = 1.5;
//         allowed = rel_thresh * pow((std::min(r0, r1) / std::pow(permission_factor, 1.5)), permission_factor);
//     }
//     else {
//         // float thresh = std::max(0.2f - p0.pos.y, 0.0f);
//         // allowed = std::max(thresh, abs_thresh);
//         allowed = std::max(rel_thresh, abs_thresh);
//     }
//     return dr <= allowed;
// }
