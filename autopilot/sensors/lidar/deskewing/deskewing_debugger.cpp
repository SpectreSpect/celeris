#include "deskewing_debugger.h"

#include "../../../../vulkan_self/keyboard_input_reciever.h"
#include "../../../../managers/manager_bundle.h"

DeskewingDebugger::DeskewingDebugger(
    ManagerBundle& manager_bundle, 
    std::filesystem::path recording_path,
    uint32_t max_point_count,
    int first_entry_id,
    int last_entry_id) 
    :   m_recording(recording_path, first_entry_id, last_entry_id),
        m_deskewer(m_odometry_estimator),
        m_original_point_cloud(manager_bundle, max_point_count),
        m_deskewed_point_cloud(manager_bundle, max_point_count) {

    for (int i = 0; i < m_recording.size(); i++) {
        LidarMessageOdometryEntry& entry = m_recording.get_entry(i);
        m_odometry_estimator.submit_odometry(entry.odometry);
    }

    m_original_point_cloud.set_color(glm::vec4(1, 0, 0, 1));
    m_deskewed_point_cloud.set_color(glm::vec4(0, 0, 1, 1));

    update_point_clouds(m_recording.get_entry(m_current_entry_id));

    add_child(m_original_point_cloud);
    add_child(m_deskewed_point_cloud);
}

void DeskewingDebugger::update(KeyboardInputReciever& keyboard_input_reciever) {
    if (keyboard_input_reciever.on_key_pressed(GLFW_KEY_N)) {
        next_entry();
    }
}
    
void DeskewingDebugger::update_point_clouds(const LidarMessageOdometryEntry& source) {
    LidarMessageOdometryEntry entry = source; // copy

    m_original_point_cloud.set_points(entry.lidar_message.points);
    m_deskewer.deskew(entry.lidar_message, entry.odometry);
    m_deskewed_point_cloud.set_points(entry.lidar_message.points);

    m_original_point_cloud.transform.position = entry.odometry.position;
    m_original_point_cloud.transform.rotation = entry.odometry.orientation;
    m_deskewed_point_cloud.transform.position = entry.odometry.position;
    m_deskewed_point_cloud.transform.rotation = entry.odometry.orientation;
}

void DeskewingDebugger::next_entry() {
    m_current_entry_id = (m_current_entry_id + 1) % m_recording.size();

    LidarMessageOdometryEntry& entry = m_recording.get_entry(m_current_entry_id);

    update_point_clouds(entry);
}