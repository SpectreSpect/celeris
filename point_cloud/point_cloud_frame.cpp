#include "point_cloud_frame.h"

PointCloudFrame::PointCloudFrame(const std::filesystem::path& path) {
    load_from_file(path);
}

void PointCloudFrame::load_from_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Failed to open: " + path.string());

    uint32_t count = 0;

    in.read(reinterpret_cast<char*>(&timestamp_ns), sizeof(uint64_t));
    in.read(reinterpret_cast<char*>(&count), sizeof(uint32_t));
    // in.read(reinterpret_cast<char*>(&flags), sizeof(uint32_t));
    if (!in) throw std::runtime_error("Bad header in: " + path.string());

    std::vector<PointInstance> local_points;
    local_points.reserve(count);

    const bool has_rgb = (flags & 1u) != 0;
    const bool has_intensity = (flags & 2u) != 0;

    size_t bpp = 10 * sizeof(float);
    // if (has_rgb) bpp += 3 * sizeof(uint8_t);
    // if (has_intensity) bpp += sizeof(float);

    std::vector<uint8_t> buf(size_t(count) * bpp);
    in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    if (!in) throw std::runtime_error("Unexpected EOF in: " + path.string());

    local_points.resize(count);

    const uint8_t* p = buf.data();
    for (uint32_t i = 0; i < count; ++i) {
        std::memcpy(&local_points[i].pos.x, p, 4); p += 4;
        std::memcpy(&local_points[i].pos.y, p, 4); p += 4;
        std::memcpy(&local_points[i].pos.z, p, 4); p += 4;
        p += 28;
        // std::memcpy(&local_points[i].time, p, 4); p += 4;
        // std::memcpy(&local_points[i].gps_pos.x, p, 4); p += 4;
        // std::memcpy(&local_points[i].gps_pos.y, p, 4); p += 4;
        // std::memcpy(&local_points[i].gps_pos.z, p, 4); p += 4;
        // std::memcpy(&local_points[i].imu_rotation.x, p, 4); p += 4;
        // std::memcpy(&local_points[i].imu_rotation.y, p, 4); p += 4;
        // std::memcpy(&local_points[i].imu_rotation.z, p, 4); p += 4;

        // std::memcpy(&local_points[i].time, p, 4); p += 4;

        // local_points[i].pos.x += local_points[i].gps_pos.x;
        // local_points[i].pos.y += local_points[i].gps_pos.z;
        // local_points[i].pos.z += local_points[i].gps_pos.y;



        // std::cout << local_points[i].time << std::endl;


        // How do I apply the gps and imu transformation to each point here?????
        
        float y = local_points[i].pos.y;
        float z = local_points[i].pos.z;

        local_points[i].pos.x = -local_points[i].pos.x;
        local_points[i].pos.y = z;
        local_points[i].pos.z = y;



        // glm::vec3 p_local_ros = local_points[i].pos;

        // // 2) pose in ROS coords (translation + RPY)
        // glm::vec3 t_ros = local_points[i].gps_pos;
        // float roll  = local_points[i].imu_rotation.x;
        // float pitch = local_points[i].imu_rotation.y;
        // float yaw   = local_points[i].imu_rotation.z;

        // // 3) transform point into "world" (or whatever GPS frame is)
        // glm::mat3 R_ros = rpy_to_mat3_zyx(roll, pitch, yaw);
        // glm::vec3 p_world_ros = R_ros * p_local_ros + t_ros;

        // // 4) convert to engine coords once
        // local_points[i].pos = ros_pos_to_engine(p_world_ros);

        

        // ros_rpy_to_engine_rpy
        


        //   glm::vec3 pos_0 = glm::vec3(-point_0.x, point_0.z, point_0.y);
        // local_points[i].color.r = local_points[i].color.g = local_points[i].color.b = 1.0f;
        
        local_points[i].color.r = local_points[i].color.g = local_points[i].color.b = local_points[i].pos.y / 3.0f;
        local_points[i].color = glm::vec4(0, 0, 1, 1);

        // if (has_rgb) {
        //     // local_points[i].color.r = *p++;
        //     // local_points[i].color.g = *p++;
        //     // local_points[i].color.b = *p++;
        // }
        // if (has_intensity) {
        //     // std::memcpy(&local_points[i].intensity, p, 4); 
        //     // p += 4;
        // }
    }

    point_cloud.get_normals(local_points, normals);
    point_cloud.remove_invalid_points_and_normals(local_points, normals);
    point_cloud.drop_out_points_and_normals(local_points, normals, 2500);

    point_cloud.remove_points_near_origin(local_points, normals, 3);

    // point_cloud.drop_out_points_and_normals(local_points, normals, 10000);
    point_cloud.update_points(std::move(local_points));
    point_cloud.get_normals_ssbo(normals, normals_ssbo);
}


// void PointCloudFrame::get_normals(std::vector<PointInstance> points, std::vector<glm::vec3> normals) {
//     int rings_count = 16;
//     int ring_width = points.size() / rings_count;
//     int cloud_size = points.size();

//     for (int y = 0; y < rings_count - 1; y++){
//         for (int x = 0; x < ring_width - 1; x++){

//         }
//     }
// }

void PointCloudFrame::draw(RenderState state) {
    state.transform *= get_model_matrix();
    point_cloud.draw(state);
}