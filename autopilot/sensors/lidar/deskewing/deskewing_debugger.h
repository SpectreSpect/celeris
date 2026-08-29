#pragma once

#include <filesystem>

#include "../../../recorder/lidar_message_odometry_recording.h"
#include "../../../../renderer/point_cloud/point_cloud.h"
#include "../../../odometry/odometry_estimator.h"
#include "../../../../renderer/scene_object.h"
#include "lidar_scan_deskewer.h"

class KeyboardInputReciever;
class ManagerBundle;

class DeskewingDebugger : public SceneObject {
public:
    DeskewingDebugger(
        ManagerBundle& manager_bundle, 
        std::filesystem::path recording_path,
        uint32_t max_point_count,
        int first_entry_id = -1,
        int last_entry_id = -1
    );

    void update(KeyboardInputReciever& keyboard_input_reciever);

private:
    OdometryEstimator m_odometry_estimator;
    LidarMessageOdometryRecordering m_recording;
    LidarScanDeskewer m_deskewer;

    PointCloud m_original_point_cloud;
    PointCloud m_deskewed_point_cloud;

    uint32_t m_current_entry_id = 0;

    void update_point_clouds(const LidarMessageOdometryEntry& source);
    void next_entry();
};