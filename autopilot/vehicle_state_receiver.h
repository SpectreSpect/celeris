#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <mutex>
#include <thread>

enum VehicleFeedbackFlags : uint32_t {
    VEHICLE_FEEDBACK_HAS_ODOMETRY = 1u << 0,
    VEHICLE_FEEDBACK_HAS_VEHICLE_STATE = 1u << 1
};

struct VehicleFeedback {
    uint64_t timestamp_ns = 0;
    uint32_t flags = 0;

    glm::vec3 position_ros{0.0f};
    glm::quat orientation_ros{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 linear_velocity_ros{0.0f};
    glm::vec3 angular_velocity_ros{0.0f};

    float speed = 0.0f;
    float acceleration = 0.0f;
    float steering_angle = 0.0f;
    float steering_angle_velocity = 0.0f;
    float steering_angle_acceleration = 0.0f;

    std::chrono::steady_clock::time_point received_at{};

    bool has_odometry() const noexcept {
        return (flags & VEHICLE_FEEDBACK_HAS_ODOMETRY) != 0u;
    }

    bool has_vehicle_state() const noexcept {
        return (flags & VEHICLE_FEEDBACK_HAS_VEHICLE_STATE) != 0u;
    }
};

class VehicleStateReceiver {
public:
    explicit VehicleStateReceiver(uint16_t port = 5002);
    ~VehicleStateReceiver();

    VehicleStateReceiver(const VehicleStateReceiver&) = delete;
    VehicleStateReceiver& operator=(const VehicleStateReceiver&) = delete;

    void start();
    void stop();

    bool latest_feedback(VehicleFeedback& feedback) const;
    bool is_running() const noexcept;
    bool is_connected() const noexcept;

private:
    uint16_t m_port = 0;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_connected{false};
    int m_listen_socket = -1;
    std::atomic<int> m_client_socket{-1};
    std::thread m_thread;

    mutable std::mutex m_feedback_mutex;
    VehicleFeedback m_latest_feedback;
    bool m_has_feedback = false;

    void receive_loop();
    bool receive_feedback_from_client(int client_socket);
    bool read_exact(int socket, void* data, size_t byte_count);
    void set_latest_feedback(VehicleFeedback feedback);
    void close_listen_socket();
};
