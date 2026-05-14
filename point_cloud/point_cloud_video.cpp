#include "point_cloud_video.h"

static inline bool parse_u64(const std::string& s, uint64_t& out) {
    try { out = std::stoull(s); return true; } catch (...) { return false; }
}
static inline bool parse_u32(const std::string& s, uint32_t& out) {
    try { out = static_cast<uint32_t>(std::stoul(s)); return true; } catch (...) { return false; }
}
static inline bool parse_f(const std::string& s, float& out) {
    try { out = std::stof(s); return true; } catch (...) { return false; }
}

// void PointCloudVideo::load_from_file(VulkanEngine& engine, const std::filesystem::path& csv_path, int max_frames) {
//     std::ifstream in(csv_path);
//     if (!in) throw std::runtime_error("Failed to open: " + csv_path.string());

//     std::vector<IndexEntry> entries;
//     std::filesystem::path video_dir_path = csv_path.parent_path();
//     std::string line;

//     int loaded_count = 0;

//     while (std::getline(in, line)) {
//         if (loaded_count > max_frames)
//             break;

//         if (line.empty()) continue;

//         std::vector<std::string> tok;
//         {
//             std::stringstream ss(line);
//             std::string t;
//             while (std::getline(ss, t, ',')) tok.push_back(t);
//         }

//         // Expect at least: id, t_ns, filename, count, px,py,pz, roll,pitch,yaw
//         if (tok.size() < 16) continue;

//         IndexEntry e;
//         if (!parse_u64(tok[0], e.frame_id)) continue;
//         if (!parse_u64(tok[1], e.timestamp_ns)) continue;
//         e.filename = tok[2];
//         if (!parse_u32(tok[3], e.point_count)) continue;

//         float px, py, pz, rr, pp, yy;
//         float angacl_x, angacl_y, angacl_z;
//         float linacl_x, linacl_y, linacl_z;
//         if (!parse_f(tok[4], px) || !parse_f(tok[5], py) || !parse_f(tok[6], pz)) continue;
//         if (!parse_f(tok[7], rr) || !parse_f(tok[8], pp) || !parse_f(tok[9], yy)) continue;
//         if (!parse_f(tok[10], angacl_x) || !parse_f(tok[11], angacl_y) || !parse_f(tok[12], angacl_z)) continue;
//         if (!parse_f(tok[13], linacl_x) || !parse_f(tok[14], linacl_y) || !parse_f(tok[15], linacl_z)) continue;

//         e.position = glm::vec3(px, py, pz);
//         e.rotation_rpy = glm::vec3(rr, pp, yy);
//         e.angular_velocity = glm::vec3(angacl_x, angacl_y, angacl_z);;
//         e.linear_acceleration = glm::vec3(linacl_x, linacl_y, linacl_z);

//         entries.push_back(std::move(e));

//         loaded_count++;
//     }

//     std::sort(entries.begin(), entries.end(),
//               [](const IndexEntry& x, const IndexEntry& y) { return x.timestamp_ns < y.timestamp_ns; });

//     frames.clear();
//     frames.reserve(entries.size());

//     for (const auto& e : entries) {
//         frames.emplace_back();
//         frames.back().load_from_file(engine, video_dir_path / e.filename);

//         frames.back().timestamp_ns = e.timestamp_ns;

//         glm::vec3 car_pos_eng = ros_pos_to_engine(e.position);
//         glm::vec3 car_rpy_eng = ros_rpy_to_engine_rpy(e.rotation_rpy);

//         frames.back().car_pos = car_pos_eng;
//         frames.back().car_rotation = car_rpy_eng;

//         glm::mat4 T_world_car = pose_to_mat4(car_pos_eng, car_rpy_eng);

//         // Replace these with your actual LiDAR mounting extrinsics
//         glm::vec3 lidar_offset_from_car_eng(0.0f, 0.0f, 0.0f);
//         glm::vec3 lidar_rpy_from_car_eng(0.0f, 0.0f, 0.0f);

//         glm::mat4 T_car_lidar = pose_to_mat4(
//             lidar_offset_from_car_eng,
//             lidar_rpy_from_car_eng
//         );

//         glm::mat4 T_world_lidar = T_world_car * T_car_lidar;

//         glm::vec3 lidar_pos_eng, lidar_rpy_eng;
//         mat4_to_pose(T_world_lidar, lidar_pos_eng, lidar_rpy_eng);

//         frames.back().point_cloud.position = lidar_pos_eng;
//         frames.back().point_cloud.rotation = lidar_rpy_eng;

//         glm::mat3 R_eng = rpy_to_mat3(
//             car_rpy_eng.x,
//             car_rpy_eng.y,
//             car_rpy_eng.z
//         );

//         frames.back().linear_acceleration =
//             R_eng * ros_vec_to_engine(e.linear_acceleration);

//         frames.back().angular_velocity = ros_vec_to_engine(e.angular_velocity);

//         frames.back().velocity = glm::vec3(0.0f);
//         frames.back().angular_velocity = glm::vec3(0.0f);
//     }
// }


void PointCloudVideo::load_from_file(VulkanEngine& engine, const std::filesystem::path& csv_path, int first_frame, int last_frame) {
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

    std::sort(entries.begin(), entries.end(),
              [](const IndexEntry& x, const IndexEntry& y) { return x.timestamp_ns < y.timestamp_ns; });

    frames.clear();
    frames.reserve(entries.size());

    for (const auto& e : entries) {
        frames.emplace_back();
        frames.back().load_from_file(engine, video_dir_path / e.filename);

        frames.back().timestamp_ns = e.timestamp_ns;

        glm::vec3 car_pos_eng = ros_pos_to_engine(e.position);
        glm::vec3 car_rpy_eng = ros_rpy_to_engine_rpy(e.rotation_rpy);

        frames.back().car_pos = car_pos_eng;
        frames.back().car_rotation = car_rpy_eng;

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

        frames.back().point_cloud.position = lidar_pos_eng;
        frames.back().point_cloud.rotation = lidar_rpy_eng;

        frames.back().point_cloud.color = glm::vec4(1, 0, 0, 1);

        glm::mat3 R_eng = rpy_to_mat3(
            car_rpy_eng.x,
            car_rpy_eng.y,
            car_rpy_eng.z
        );

        frames.back().linear_acceleration =
            R_eng * ros_vec_to_engine(e.linear_acceleration);

        frames.back().angular_velocity = ros_vec_to_engine(e.angular_velocity);

        frames.back().velocity = glm::vec3(0.0f);
        frames.back().angular_velocity = glm::vec3(0.0f);
    }
}

size_t PointCloudVideo::get_frame_id(float time, size_t search_start_id) {
    int frame_id = 0;
    for (int i = search_start_id; i < frames.size(); i++) {
        double time_seconds = (double)frames[i].timestamp_ns / 1000000000.0f;

        if (time_seconds > timer)
            return frame_id;
        else
            frame_id = i;
    }

    return frame_id;
}

void PointCloudVideo::move(float time) {
    timer = time;
    current_frame = get_frame_id(timer);
}

void PointCloudVideo::pause() {
    paused = true;
}

void PointCloudVideo::resume() {
    paused = false;
}

void PointCloudVideo::update(float delta_time) {
    if (paused) return;

    if (looped && current_frame == frames.size() - 1) {
        current_frame = 0;
        timer = 0.0f;
    }

    timer += delta_time;
    current_frame = get_frame_id(timer, current_frame);
}

void PointCloudVideo::draw(RenderState state) {
    state.transform *= get_model_matrix();

    if (current_frame > frames.size())
        throw std::out_of_range("PointCloudVideo::draw");
    
    // frames[current_frame].draw(state);

    // for (int i = 0; i <= current_frame; i++)
    //     frames[i].draw(state);

    // frames[0].draw(state);
    // frames[250].draw(state);


}

void PointCloudVideo::draw_clouds(PointCloudPass& point_cloud_pass, Camera& camera) {
    if (current_frame > frames.size())
        throw std::out_of_range("PointCloudVideo::draw");
    
    point_cloud_pass.render(frames[current_frame].point_cloud, camera);
}