#include "lidar_video.h"

#include <fstream>
#include <algorithm>

#include "../point_instance.h"


LidarVideo::LidarVideo(ManagerBundle& manager_bundle, const std::filesystem::path& csv_path, int first_frame, int last_frame){
    load_from_file(manager_bundle, csv_path, first_frame, last_frame);
}

void LidarVideo::load_from_file(ManagerBundle& manager_bundle, const std::filesystem::path& csv_path, int first_frame, int last_frame) {
    std::ifstream in(csv_path);
    if (!in) throw std::runtime_error("Failed to open: " + csv_path.string());

    std::vector<IndexEntry> entries;
    std::filesystem::path video_dir_path = csv_path.parent_path();
    std::string line;

    int loaded_count = 0;
    int line_id = -1;
    // int loaded_count = first_frame >= 0 ? first_frame : 0;

    while (std::getline(in, line)) {
        line_id++;

        if (first_frame >= 0)
            if (line_id < first_frame)
                continue;
        
        if (last_frame >= 0)
            if (line_id > last_frame)
                break;
        
        
        // if (loaded_count > max_frames)
        //     break;

        if (line.empty()) continue;

        std::vector<std::string> tok;
        {
            std::stringstream ss(line);
            std::string t;
            while (std::getline(ss, t, ',')) tok.push_back(t);
        }

        // Expect at least: id, t_ns, filename, count, px,py,pz, roll,pitch,yaw
        if (tok.size() < 16) continue;

        IndexEntry e;
        if (!parse_u64(tok[0], e.frame_id)) continue;
        if (!parse_u64(tok[1], e.timestamp_ns)) continue;
        e.filename = tok[2];
        if (!parse_u32(tok[3], e.point_count)) continue;

        float px, py, pz, rr, pp, yy;
        float angacl_x, angacl_y, angacl_z;
        float linacl_x, linacl_y, linacl_z;
        if (!parse_f(tok[4], px) || !parse_f(tok[5], py) || !parse_f(tok[6], pz)) continue;
        if (!parse_f(tok[7], rr) || !parse_f(tok[8], pp) || !parse_f(tok[9], yy)) continue;
        if (!parse_f(tok[10], angacl_x) || !parse_f(tok[11], angacl_y) || !parse_f(tok[12], angacl_z)) continue;
        if (!parse_f(tok[13], linacl_x) || !parse_f(tok[14], linacl_y) || !parse_f(tok[15], linacl_z)) continue;

        e.position = glm::vec3(px, py, pz);
        e.rotation_rpy = glm::vec3(rr, pp, yy);
        e.angular_velocity = glm::vec3(angacl_x, angacl_y, angacl_z);;
        e.linear_acceleration = glm::vec3(linacl_x, linacl_y, linacl_z);

        entries.push_back(std::move(e));

        loaded_count++;
    }

    std::sort(entries.begin(), entries.end(), [](const IndexEntry& x, const IndexEntry& y) { return x.timestamp_ns < y.timestamp_ns; });

    m_scans.clear();
    m_frame_timestamps_ns.clear();

    m_scans.reserve(entries.size());
    m_frame_timestamps_ns.reserve(entries.size());

    for (const auto& e : entries) {
        // LidarScan* lidar_scan = new LidarScan(manager_bundle, video_dir_path / e.filename);

        m_scans.emplace_back(manager_bundle, video_dir_path / e.filename);

        m_scans.back().set_timestamp_ns(e.timestamp_ns);
        m_frame_timestamps_ns.push_back(e.timestamp_ns);

        glm::vec3 car_pos_eng = LidarScan::ros_pos_to_engine(e.position);
        glm::vec3 car_rpy_eng = ros_rpy_to_engine_rpy(e.rotation_rpy);

        // frames.back().car_pos = car_pos_eng;
        // frames.back().car_rotation = car_rpy_eng;

        glm::mat4 T_world_car = pose_to_mat4(car_pos_eng, car_rpy_eng);

        // Replace these with your actual LiDAR mounting extrinsics
        glm::vec3 lidar_offset_from_car_eng(0.0f, 0.0f, 0.0f);
        glm::vec3 lidar_rpy_from_car_eng(0.0f, 0.0f, 0.0f);

        glm::mat4 T_car_lidar = pose_to_mat4(
            lidar_offset_from_car_eng,
            lidar_rpy_from_car_eng
        );

        glm::mat4 T_world_lidar = T_world_car * T_car_lidar;

        glm::vec3 lidar_pos_eng, lidar_rpy_eng;
        mat4_to_pose(T_world_lidar, lidar_pos_eng, lidar_rpy_eng);

        m_scans.back().point_cloud().transform.position = lidar_pos_eng;
        m_scans.back().point_cloud().transform.rotation = lidar_rpy_eng;

        // frames.back().point_cloud.color = glm::vec4(1, 0, 0, 1);

        // glm::mat3 R_eng = rpy_to_mat3(
        //     car_rpy_eng.x,
        //     car_rpy_eng.y,
        //     car_rpy_eng.z
        // );

        // frames.back().linear_acceleration =
        //     R_eng * ros_vec_to_engine(e.linear_acceleration);

        // frames.back().angular_velocity = ros_vec_to_engine(e.angular_velocity);

        // frames.back().velocity = glm::vec3(0.0f);
        // frames.back().angular_velocity = glm::vec3(0.0f);
    }

    m_timer = 0.0f;

    if (!m_scans.empty())
        set_frame(0);
}

bool LidarVideo::parse_u64(const std::string& s, uint64_t& out) {
    try { out = std::stoull(s); return true; } catch (...) { return false; }
}

bool LidarVideo::parse_u32(const std::string& s, uint32_t& out) {
    try { out = static_cast<uint32_t>(std::stoul(s)); return true; } catch (...) { return false; }
}

bool LidarVideo::parse_f(const std::string& s, float& out) {
    try { out = std::stof(s); return true; } catch (...) { return false; }
}

void LidarVideo::mat4_to_pose(const glm::mat4& M, glm::vec3& position, glm::vec3& rotation) {
    position = glm::vec3(M[3]);
    rotation = mat3_to_euler_xyz_custom(glm::mat3(M));
}

glm::vec3 LidarVideo::ros_vec_to_engine(const glm::vec3& v_ros) {
    return basis_M_ros_to_engine() * v_ros;
}

glm::vec3 LidarVideo::mat3_to_euler_xyz_custom(const glm::mat3& R) {
        const float EPS = 1e-6f;
        const float HALF_PI = 1.57079632679f;

        float x, y, z;
        float sy = glm::clamp(-R[0][2], -1.0f, 1.0f);

        if (sy >= 1.0f - EPS) {
            y = HALF_PI;
            z = 0.0f;
            x = std::atan2(R[1][0], R[1][1]);
        }
        else if (sy <= -1.0f + EPS) {
            y = -HALF_PI;
            z = 0.0f;
            x = std::atan2(-R[1][0], R[1][1]);
        }
        else {
            y = std::asin(sy);
            x = std::atan2(R[1][2], R[2][2]);
            z = std::atan2(R[0][1], R[0][0]);
        }

        return glm::vec3(x, y, z);
    }

glm::mat4 LidarVideo::pose_to_mat4(const glm::vec3& position, const glm::vec3& rotation) {
    glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), rotation.x, glm::vec3(1, 0, 0));
    glm::mat4 Ry = glm::rotate(glm::mat4(1.0f), rotation.y, glm::vec3(0, 1, 0));
    glm::mat4 Rz = glm::rotate(glm::mat4(1.0f), rotation.z, glm::vec3(0, 0, 1));
    glm::mat4 R  = Rz * Ry * Rx;
    glm::mat4 T  = glm::translate(glm::mat4(1.0f), position);
    return T * R;
}

glm::vec3 LidarVideo::mat3_to_rpy_zyx(const glm::mat3& R) {
    // Using the standard ZYX (yaw-pitch-roll) extraction
    // Beware GLM indexing: R[col][row]
    float r20 = R[0][2]; // row2 col0
    float r10 = R[0][1]; // row1 col0
    float r00 = R[0][0]; // row0 col0
    float r21 = R[1][2]; // row2 col1
    float r22 = R[2][2]; // row2 col2

    float pitch = std::asin(glm::clamp(-r20, -1.0f, 1.0f));
    float yaw   = std::atan2(r10, r00);
    float roll  = std::atan2(r21, r22);

    return glm::vec3(roll, pitch, yaw);
}

glm::mat3 LidarVideo::rpy_to_mat3(float roll, float pitch, float yaw) {
    glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), roll,  glm::vec3(1,0,0));
    glm::mat4 Ry = glm::rotate(glm::mat4(1.0f), pitch, glm::vec3(0,1,0));
    glm::mat4 Rz = glm::rotate(glm::mat4(1.0f), yaw,   glm::vec3(0,0,1));
    return glm::mat3(Rz * Ry * Rx);
}

glm::mat3 LidarVideo::basis_M_ros_to_engine() {
    // Columns are images of ROS basis vectors expressed in engine coords
    // col0 = M*[1,0,0] = (-1,0,0)
    // col1 = M*[0,1,0] = (0,0,1)
    // col2 = M*[0,0,1] = (0,1,0)
    glm::mat3 M(1.0f);
    M[0] = glm::vec3(-1, 0, 0);
    M[1] = glm::vec3( 0, 0, 1);
    M[2] = glm::vec3( 0, 1, 0);
    return M;
}

glm::vec3 LidarVideo::ros_rpy_to_engine_rpy(const glm::vec3& rpy_ros) {
    glm::mat3 R_ros = rpy_to_mat3(rpy_ros.x, rpy_ros.y, rpy_ros.z);

    glm::mat3 M = basis_M_ros_to_engine();
    glm::mat3 R_eng = M * R_ros * glm::transpose(M); // change of basis

    return mat3_to_rpy_zyx(R_eng);
}

uint32_t LidarVideo::current_frame_id() const {
    return m_current_frame_id;
}

void LidarVideo::next_frame() {
    LOG_METHOD();

    if (m_scans.empty())
        return;

    m_current_frame_id = (m_current_frame_id + 1) % m_scans.size();

    sync();
}

void LidarVideo::previous_frame() {
    LOG_METHOD();

    if (m_scans.empty())
        return;

    if (m_current_frame_id == 0)
        m_current_frame_id = m_scans.size() - 1;
    else
        m_current_frame_id--;

    sync();
}

void LidarVideo::set_frame(uint32_t id) {
    LOG_METHOD();

    logger.check(id >= 0 && id < m_scans.size(), "Lidar video frame index was out of bounds");

    m_current_frame_id = id;

    sync();
}

LidarScan& LidarVideo::get_scan(uint32_t scan_id) {
    LOG_METHOD();

    logger.check(scan_id >= 0 && scan_id < m_scans.size(), "Lidar video frame index was out of bounds");

    return m_scans[scan_id];
}

uint32_t LidarVideo::get_scan_count() {
    return m_scans.size();
}

void LidarVideo::sync() {
    LOG_METHOD();

    logger.check(m_current_frame_id >= 0 && m_current_frame_id < m_scans.size(), "Lidar video frame index was out of bounds");

    if (children.empty())
        children.push_back(&m_scans[m_current_frame_id]);
    else
        children[0] = &m_scans[m_current_frame_id];
}

float LidarVideo::frame_time_seconds(size_t frame_id) const {
    if (m_frame_timestamps_ns.empty())
        return 0.0f;

    if (frame_id >= m_frame_timestamps_ns.size())
        frame_id = m_frame_timestamps_ns.size() - 1;

    const uint64_t base_ns = m_frame_timestamps_ns.front();
    const uint64_t frame_ns = m_frame_timestamps_ns[frame_id];

    return static_cast<float>(
        static_cast<double>(frame_ns - base_ns) / 1'000'000'000.0
    );
}

size_t LidarVideo::get_frame_id(float time, size_t search_start_id) const {
    if (m_scans.empty() || m_frame_timestamps_ns.empty())
        return 0;

    if (time <= 0.0f)
        return 0;

    if (search_start_id >= m_frame_timestamps_ns.size())
        search_start_id = 0;

    // If time moved backwards, search from the beginning.
    if (frame_time_seconds(search_start_id) > time)
        search_start_id = 0;

    size_t frame_id = search_start_id;

    for (size_t i = search_start_id; i < m_frame_timestamps_ns.size(); ++i) {
        const float frame_time = frame_time_seconds(i);

        if (frame_time > time)
            break;

        frame_id = i;
    }

    return frame_id;
}

void LidarVideo::move(float time) {
    if (m_scans.empty())
        return;

    m_timer = std::max(0.0f, time);

    const size_t frame_id = get_frame_id(m_timer, 0);
    set_frame(static_cast<uint32_t>(frame_id));
}

void LidarVideo::pause() {
    m_paused = true;
}

void LidarVideo::resume() {
    m_paused = false;
}

void LidarVideo::set_looped(bool looped) {
    m_looped = looped;
}

bool LidarVideo::is_paused() const {
    return m_paused;
}

void LidarVideo::update(float delta_time) {
    if (m_paused)
        return;

    if (m_scans.empty() || m_frame_timestamps_ns.empty())
        return;

    if (delta_time <= 0.0f)
        return;

    const float duration = frame_time_seconds(m_frame_timestamps_ns.size() - 1);

    m_timer += delta_time;

    size_t search_start_id = m_current_frame_id;

    if (duration > 0.0f) {
        if (m_looped) {
            while (m_timer > duration)
                m_timer -= duration;

            // Timer wrapped, so search from the beginning.
            if (m_timer < frame_time_seconds(m_current_frame_id))
                search_start_id = 0;
        }
        else {
            if (m_timer >= duration) {
                m_timer = duration;
                set_frame(static_cast<uint32_t>(m_scans.size() - 1));
                return;
            }
        }
    }

    const size_t new_frame_id = get_frame_id(m_timer, search_start_id);

    if (new_frame_id != m_current_frame_id)
        set_frame(static_cast<uint32_t>(new_frame_id));
}