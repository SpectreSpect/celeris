#include "lidar_scan.h"
#include "../point_instance.h"
#include "../../manager_bundle.h"

LidarScan::LidarScan(ManagerBundle& manager_bundle, const std::filesystem::path& path) 
    :   m_point_cloud(load_from_file(manager_bundle, path)) {
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

    points.clear();
    points.resize(count);

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
            points[i].pos = glm::vec4(INF, INF, INF, 1.0f);
            // points[i].color = glm::vec4(0, 0, 1, 1);
            points[i].color = glm::vec4(1, 1, 1, 1);
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

        points[i].pos = glm::vec4(p_ref_eng, 1.0f);
        // points[i].color = glm::vec4(0, 0, 1, 1);
        points[i].color = glm::vec4(1, 1, 1, 1);
    }

    // get_normals(points, normals);
    // remove_invalid_points_and_normals(points, normals);
    // drop_out_points_and_normals(points, normals, 10000);
    // remove_points_near_origin(points, normals, 3);

    // point_cloud.create(engine);
    // point_cloud.set_points(std::move(points));

    // normal_buffer.create(engine, normals.size() * sizeof(glm::vec4));
    // normal_buffer.update_data(normals.data(), normals.size() * sizeof(glm::vec4));

    return PointCloud(manager_bundle, points);
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