#include "lidar_scan.h"

#include <numeric>
#include <random>

#include "../point_instance.h"
#include "../../../managers/manager_bundle.h"


LidarScan::LidarScan(ManagerBundle& manager_bundle, const std::filesystem::path& path) 
    :   m_point_cloud(load_from_file(manager_bundle, path)),
        m_normal_buffer(VulkanBuffer::create_host_visible_storage_buffer(manager_bundle.engine(), m_normals.size() * sizeof(glm::vec4)))
{
    m_normal_buffer.upload(m_normals);
    add_child(m_point_cloud);
}

void LidarScan::set_timestamp_ns(uint32_t timestamp_ns) {
    m_timestamp_ns = timestamp_ns;
}

PointCloud LidarScan::load_from_file(ManagerBundle& manager_bundle, const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Failed to open: " + path.string());

    uint32_t count = 0;

    in.read(reinterpret_cast<char*>(&m_timestamp_ns), sizeof(uint64_t));
    in.read(reinterpret_cast<char*>(&count), sizeof(uint32_t));
    if (!in) throw std::runtime_error("Bad header in: " + path.string());

    struct TimedPointSample {
        glm::vec3 p_local_ros{0.0f};   // raw lidar point in lidar frame, ROS coords
        float time = 0.0f;             // per-point time
        glm::vec3 base_pos_ros{0.0f};  // interpolated GPS/base pose at point time
        glm::vec3 base_rpy_ros{0.0f};  // interpolated IMU RPY at point time
        bool valid = true;
    };

    const size_t bpp = 10 * sizeof(float); // x y z time px py pz roll pitch yaw

    std::vector<uint8_t> buf(size_t(count) * bpp);
    in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    if (!in) throw std::runtime_error("Unexpected EOF in: " + path.string());

    std::vector<TimedPointSample> samples(count);

    const float INF = std::numeric_limits<float>::infinity();
    const uint8_t* p = buf.data();

    float min_time = std::numeric_limits<float>::infinity();
    size_t ref_idx = 0;

    for (uint32_t i = 0; i < count; ++i) {
        float x, y, z;
        float time;
        float px, py, pz;
        float roll, pitch, yaw;

        std::memcpy(&x,     p, 4); p += 4;
        std::memcpy(&y,     p, 4); p += 4;
        std::memcpy(&z,     p, 4); p += 4;
        std::memcpy(&time,  p, 4); p += 4;
        std::memcpy(&px,    p, 4); p += 4;
        std::memcpy(&py,    p, 4); p += 4;
        std::memcpy(&pz,    p, 4); p += 4;
        std::memcpy(&roll,  p, 4); p += 4;
        std::memcpy(&pitch, p, 4); p += 4;
        std::memcpy(&yaw,   p, 4); p += 4;

        samples[i].p_local_ros = glm::vec3(x, y, z);
        samples[i].time = time;
        samples[i].base_pos_ros = glm::vec3(px, py, pz);
        samples[i].base_rpy_ros = glm::vec3(roll, pitch, yaw);
        samples[i].valid = std::isfinite(x) && std::isfinite(y) && std::isfinite(z);

        if (time < min_time) {
            min_time = time;
            ref_idx = i;
        }
    }

    m_points.clear();
    m_points.resize(count);

    // If your saved pose is base_link pose and LiDAR is offset from base_link,
    // put the real extrinsics here. If pose already describes the LiDAR frame,
    // keep these zero.
    const glm::vec3 lidar_offset_from_base_ros(0.0f, 0.0f, 0.0f);
    const glm::vec3 lidar_rpy_from_base_ros(0.0f, 0.0f, 0.0f);

    const glm::mat3 R_bl = rpy_to_mat3_zyx(
        lidar_rpy_from_base_ros.x,
        lidar_rpy_from_base_ros.y,
        lidar_rpy_from_base_ros.z
    );

    const TimedPointSample& ref = samples[ref_idx];

    const glm::mat3 R_wb_ref = rpy_to_mat3_zyx(
        ref.base_rpy_ros.x,
        ref.base_rpy_ros.y,
        ref.base_rpy_ros.z
    );
    const glm::mat3 R_wl_ref = R_wb_ref * R_bl;
    const glm::vec3 t_wl_ref = ref.base_pos_ros + R_wb_ref * lidar_offset_from_base_ros;

    for (uint32_t i = 0; i < count; ++i) {
        const TimedPointSample& s = samples[i];

        if (!s.valid) {
            m_points[i].pos = glm::vec4(INF, INF, INF, 1.0f);
            m_points[i].color = glm::vec4(0, 0, 0, 1);
            continue;
        }

        const glm::mat3 R_wb = rpy_to_mat3_zyx(
            s.base_rpy_ros.x,
            s.base_rpy_ros.y,
            s.base_rpy_ros.z
        );
        const glm::mat3 R_wl = R_wb * R_bl;
        const glm::vec3 t_wl = s.base_pos_ros + R_wb * lidar_offset_from_base_ros;

        // Point in world ROS
        const glm::vec3 p_world_ros = R_wl * s.p_local_ros + t_wl;

        // Bring it back into the reference LiDAR frame
        const glm::vec3 p_ref_ros = glm::transpose(R_wl_ref) * (p_world_ros - t_wl_ref);

        // Convert reference-frame point to engine coords
        const glm::vec3 p_ref_eng = ros_pos_to_engine(p_ref_ros);

        m_points[i].pos = glm::vec4(p_ref_eng, 1.0f);
        m_points[i].color = glm::vec4(0, 0, 0, 1);
    }

    get_normals(m_points, m_normals);

    return PointCloud(manager_bundle, m_points);
}

std::vector<glm::vec4> LidarScan::calculate_normals(std::vector<PointInstance> points) {
    std::vector<glm::vec4> normals;

    get_normals(m_points, normals);
    remove_invalid_points_and_normals(m_points, normals);
    drop_out_points_and_normals(m_points, normals, 10000);
    remove_points_near_origin(m_points, normals, 3);

    return normals;
}

glm::mat3 LidarScan::rpy_to_mat3_zyx(float roll, float pitch, float yaw)
    {
        // R = Rz(yaw) * Ry(pitch) * Rx(roll)
        const float cr = std::cos(roll),  sr = std::sin(roll);
        const float cp = std::cos(pitch), sp = std::sin(pitch);
        const float cy = std::cos(yaw),   sy = std::sin(yaw);

        glm::mat3 Rx(1.0f);
        Rx[0] = glm::vec3(1, 0, 0);
        Rx[1] = glm::vec3(0, cr, sr);
        Rx[2] = glm::vec3(0, -sr, cr);

        glm::mat3 Ry(1.0f);
        Ry[0] = glm::vec3(cp, 0, -sp);
        Ry[1] = glm::vec3(0, 1, 0);
        Ry[2] = glm::vec3(sp, 0, cp);

        glm::mat3 Rz(1.0f);
        Rz[0] = glm::vec3(cy, sy, 0);
        Rz[1] = glm::vec3(-sy, cy, 0);
        Rz[2] = glm::vec3(0, 0, 1);

        return Rz * Ry * Rx;
    }

glm::vec3 LidarScan::ros_pos_to_engine(const glm::vec3& p_ros)
{
    return glm::vec3(-p_ros.x, p_ros.z, p_ros.y); // (-x, z, y)
}
    
PointCloud& LidarScan::point_cloud() {
    return m_point_cloud;
}

VulkanBuffer& LidarScan::normal_buffer() {
    return m_normal_buffer;
}

LidarScan LidarScan::from_preprocess(ManagerBundle& manager_bundle, const std::filesystem::path& path) {
    LOG_NAMED("LidarScan");

    LidarScan lidar_scan(manager_bundle, path);
    lidar_scan.to_preprocess_points();
    return lidar_scan;
}

void LidarScan::remove_points_near_origin(std::vector<PointInstance>& points,
                                           std::vector<glm::vec4>& normals,
                                           float min_distance)
{
    if (points.size() != normals.size()) {
        std::cout << "remove_points_near_origin: points.size() != normals.size()\n";
        return;
    }

    float min_dist_sq = min_distance * min_distance;

    std::vector<PointInstance> filtered_points;
    std::vector<glm::vec4> filtered_normals;

    filtered_points.reserve(points.size());
    filtered_normals.reserve(normals.size());

    for (size_t i = 0; i < points.size(); i++) {
        const PointInstance& p = points[i];
        const glm::vec4& n = normals[i];

        glm::vec3 pos = glm::vec3(p.pos);
        float dist_sq = glm::dot(pos, pos);

        // remove points that are too close to the lidar origin
        // if (dist_sq < min_dist_sq || p.pos.y < 1.0)
        if (dist_sq < min_dist_sq)
            continue;

        filtered_points.push_back(p);
        filtered_normals.push_back(n);
    }

    points = std::move(filtered_points);
    normals = std::move(filtered_normals);
}

void LidarScan::to_preprocess_points() {
    LOG_METHOD();

    remove_invalid_points_and_normals(m_points, m_normals);
    drop_out_points_and_normals(m_points, m_normals, 10000);
    remove_points_near_origin(m_points, m_normals, 3);
}

void LidarScan::drop_out_points_and_normals(std::vector<PointInstance>& points,
                                             std::vector<glm::vec4>& normals,
                                             size_t target_size)
{
    if (points.size() != normals.size()) {
        std::cout << "drop_out_points_and_normals: points.size() != normals.size()\n";
        return;
    }

    size_t n = points.size();

    if (target_size >= n) {
        return; // nothing to drop
    }

    if (target_size == 0) {
        points.clear();
        normals.clear();
        return;
    }

    // Create indices [0, 1, 2, ..., n-1]
    std::vector<size_t> indices(n);
    std::iota(indices.begin(), indices.end(), 0);

    // Shuffle indices randomly
    static std::random_device rd;
    static std::mt19937 rng(rd());
    std::shuffle(indices.begin(), indices.end(), rng);

    // Keep only the first target_size indices
    indices.resize(target_size);

    // Optional: sort so the remaining points keep their original relative order
    std::sort(indices.begin(), indices.end());

    std::vector<PointInstance> new_points;
    std::vector<glm::vec4> new_normals;

    new_points.reserve(target_size);
    new_normals.reserve(target_size);

    for (size_t idx : indices) {
        new_points.push_back(points[idx]);
        new_normals.push_back(normals[idx]);
    }

    points = std::move(new_points);
    normals = std::move(new_normals);
}


void LidarScan::keep_only_upward_facing_points_and_normals(
    std::vector<PointInstance>& points,
    std::vector<glm::vec4>& normals,
    float up_dot_threshold)
{
    if (points.size() != normals.size()) {
        std::cout << "keep_only_upward_facing_points_and_normals: points.size() != normals.size()\n";
        return;
    }

    std::vector<PointInstance> kept_points;
    std::vector<glm::vec4> kept_normals;

    kept_points.reserve(points.size());
    kept_normals.reserve(normals.size());

    const glm::vec3 up(0.0f, 1.0f, 0.0f);

    for (size_t i = 0; i < points.size(); ++i) {
        const PointInstance& p = points[i];
        const glm::vec3 n = glm::vec3(normals[i]);

        if (!is_point_valid(p) || glm::dot(n, n) < 1e-12f) {
            continue;
        }

        float up_dot = glm::dot(glm::normalize(n), up);

        if (up_dot >= up_dot_threshold) {
            kept_points.push_back(p);
            kept_normals.push_back(normals[i]);
        }
    }

    points = std::move(kept_points);
    normals = std::move(kept_normals);
}



void LidarScan::remove_ground_points_and_normals(
    std::vector<PointInstance>& points,
    std::vector<glm::vec4>& normals,
    float up_dot_threshold,
    float max_ground_height)
{
    if (points.size() != normals.size()) {
        std::cout << "remove_ground_points_and_normals: points.size() != normals.size()\n";
        return;
    }

    std::vector<PointInstance> filtered_points;
    std::vector<glm::vec4> filtered_normals;

    filtered_points.reserve(points.size());
    filtered_normals.reserve(normals.size());

    const glm::vec3 up(0.0f, 1.0f, 0.0f);

    for (size_t i = 0; i < points.size(); ++i) {
        const PointInstance& p = points[i];
        const glm::vec3 n = glm::vec3(normals[i]);

        // Keep invalid points/normals handling simple
        if (!is_point_valid(p) || glm::dot(n, n) < 1e-12f) {
            filtered_points.push_back(p);
            filtered_normals.push_back(normals[i]);
            continue;
        }

        glm::vec3 nn = glm::normalize(n);
        float up_dot = glm::dot(nn, up);

        bool looks_like_ground =
            (up_dot >= up_dot_threshold) &&
            (p.pos.y <= max_ground_height);

        if (!looks_like_ground) {
            filtered_points.push_back(p);
            filtered_normals.push_back(normals[i]);
        }
    }

    points = std::move(filtered_points);
    normals = std::move(filtered_normals);
}


void LidarScan::get_normals(const std::vector<PointInstance>& points, std::vector<glm::vec4>& normals)
{
    normals.clear();
    normals.resize(points.size(), glm::vec4(0.0f));

    if (points.empty())
        return;

    const int rings_count = 16;
    const int cloud_size = static_cast<int>(points.size());

    if (cloud_size < rings_count)
        return;

    const int ring_width = cloud_size / rings_count;
    if (ring_width < 2)
        return;

    const float rel_thresh = 1.0f;

    auto accumulate_triangle_normal = [&](int ia, int ib, int ic)
    {
        const PointInstance& a = points[ia];
        const PointInstance& b = points[ib];
        const PointInstance& c = points[ic];

        // Extra safety: don't accumulate into invalid points
        if (!is_point_valid(a) || !is_point_valid(b) || !is_point_valid(c))
            return;

        glm::vec3 n = triangle_normal(a, b, c);

        if (glm::dot(n, n) < 1e-12f)
            return;

        // Keep normals consistently oriented upward
        if (glm::dot(n, glm::vec3(0.0f, 1.0f, 0.0f)) < 0.0f)
            n = -n;

        glm::vec4 n4(n, 0.0f);

        normals[ia] += n4;
        normals[ib] += n4;
        normals[ic] += n4;
    };

    for (int y = 0; y < rings_count - 1; y++) {
        for (int x = 0; x < ring_width - 1; x++) {
            int id1 = xy_id(x,     y,     ring_width, cloud_size);
            int id2 = xy_id(x,     y + 1, ring_width, cloud_size);
            int id3 = xy_id(x + 1, y + 1, ring_width, cloud_size);
            int id5 = xy_id(x + 1, y,     ring_width, cloud_size);

            const PointInstance& p0 = points[id1]; // lower-left
            const PointInstance& p1 = points[id2]; // upper-left
            const PointInstance& p2 = points[id3]; // upper-right
            const PointInstance& p3 = points[id5]; // lower-right

            bool tri1_ok = false;
            bool tri2_ok = false;

            if (is_point_valid(p0) && is_point_valid(p1) && is_point_valid(p2)) {
                if (is_same_object(p0, p1, rel_thresh) &&
                    is_same_object(p1, p2, 1.5f, false))
                {
                    tri1_ok = true;
                }
            }

            if (is_point_valid(p2) && is_point_valid(p3) && is_point_valid(p0)) {
                if (is_same_object(p3, p2, rel_thresh) &&
                    is_same_object(p3, p0, 1.5f, false))
                {
                    tri2_ok = true;
                }
            }

            if (tri1_ok) {
                accumulate_triangle_normal(id3, id1, id2);
            }

            if (tri2_ok) {
                accumulate_triangle_normal(id1, id3, id5);
            }
        }
    }

    // Normalize only valid points that actually accumulated something.
    // Invalid points remain degenerate: (0,0,0,0).
    for (size_t i = 0; i < normals.size(); i++) {
        if (!is_point_valid(points[i])) {
            normals[i] = glm::vec4(0.0f);
            continue;
        }

        glm::vec3 n = glm::vec3(normals[i]);
        float len2 = glm::dot(n, n);

        if (len2 < 1e-12f) {
            normals[i] = glm::vec4(0.0f);
        } else {
            normals[i] = glm::vec4(glm::normalize(n), 0.0f);
        }
    }
}

void LidarScan::remove_invalid_points_and_normals(std::vector<PointInstance>& points,
                                                   std::vector<glm::vec4>& normals)
{
    if (points.size() != normals.size()) {
        std::cout << "remove_invalid_points_and_normals: points.size() != normals.size()\n";
        return;
    }

    std::vector<PointInstance> filtered_points;
    std::vector<glm::vec4> filtered_normals;

    filtered_points.reserve(points.size());
    filtered_normals.reserve(normals.size());

    for (size_t i = 0; i < points.size(); i++) {
        const PointInstance& p = points[i];
        const glm::vec4& n4 = normals[i];
        glm::vec3 n = glm::vec3(n4);

        bool point_valid = is_point_valid(p);
        bool normal_valid = glm::dot(n, n) > 1e-12f;

        if (!point_valid || !normal_valid)
            continue;

        filtered_points.push_back(p);
        filtered_normals.push_back(glm::vec4(glm::normalize(n), 0.0f));
    }

    points = std::move(filtered_points);
    normals = std::move(filtered_normals);
}



// void LidarScan::get_normals(std::vector<PointInstance> points, std::vector<glm::vec3> normals) {
//     int rings_count = 16;
//     int ring_width = points.size() / rings_count;
//     int cloud_size = points.size();

//     for (int y = 0; y < rings_count - 1; y++){
//         for (int x = 0; x < ring_width - 1; x++){

//         }
//     }
// }

// void LidarScan::draw(RenderState state) {
//     state.transform *= get_model_matrix();
//     point_cloud.draw(state);
// }

bool LidarScan::is_point_valid(const PointInstance &p) {
    return std::isfinite(p.pos.x) && std::isfinite(p.pos.y) && std::isfinite(p.pos.z);
}

glm::vec3 LidarScan::triangle_normal(const PointInstance& a, const PointInstance& b, const PointInstance& c) {
    glm::vec3 av = {a.pos.x, a.pos.y, a.pos.z};
    glm::vec3 bv = {b.pos.x, b.pos.y, b.pos.z};
    glm::vec3 cv = {c.pos.x, c.pos.y, c.pos.z};

    glm::vec3 u = bv - av;
    glm::vec3 v = cv - av;
    glm::vec3 n = glm::cross(u, v);           // unnormalized normal (also proportional to triangle area)
    return glm::normalize(n);       // returns {0,0,0} if degenerate
}

int LidarScan::xy_id(int x, int y, int ring_width, int cloud_size) {
    int idx = x + y * ring_width;

    if (idx < 0 || idx >= cloud_size) {
        throw "Bad index";
        return -1;
    }

    return idx;
}

float LidarScan::radial_distance(const PointInstance &p) {
    return std::hypot(static_cast<double>(p.pos.x), static_cast<double>(p.pos.y), static_cast<double>(p.pos.z));
}

bool LidarScan::is_same_object(
    const PointInstance &p0,
    const PointInstance &p1,
    float rel_thresh,
    bool more_permissive_with_distance,
    float abs_thresh)
{
    if (!is_point_valid(p0) || !is_point_valid(p1))
        return false;

    float r0 = radial_distance(p0);
    float r1 = radial_distance(p1);

    if (!std::isfinite(r0) || !std::isfinite(r1))
        return false;

    float allowed = 0;
    float dr = std::fabs(r0 - r1);


    if (more_permissive_with_distance) {
        // float thresh = std::max(0.2f - p0.pos.y, 0.0f);
        // allowed = std::max(thresh * std::min(r0, r1), abs_thresh);
        float permission_factor = 1.5;
        allowed = rel_thresh * pow((std::min(r0, r1) / std::pow(permission_factor, 1.5)), permission_factor);
    }
    else {
        // float thresh = std::max(0.2f - p0.pos.y, 0.0f);
        // allowed = std::max(thresh, abs_thresh);
        allowed = std::max(rel_thresh, abs_thresh);
    }
    return dr <= allowed;
}

