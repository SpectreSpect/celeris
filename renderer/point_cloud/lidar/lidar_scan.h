#pragma once

#include "../../scene_object.h"
#include "../point_cloud.h"
#include "../../../path_utils.h"

class ManagerBundle;

class LidarScan : public SceneObject {
public:
    LidarScan(ManagerBundle& manager_bundle, const std::filesystem::path& path);


private:
    uint64_t timestamp_ns = 0;

    PointCloud load_from_file(ManagerBundle& manager_bundle, const std::filesystem::path& path); 

    static glm::mat3 rpy_to_mat3_zyx(float roll, float pitch, float yaw);
    static glm::vec3 ros_pos_to_engine(const glm::vec3& p_ros);

    std::vector<PointInstance> points;
    PointCloud point_cloud;
};