#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

struct VehicleCommand {
    float acceleration = 0.0f;
    float steering_angle_velocity = 0.0f;
};

class VehicleCommandSender {
public:
    explicit VehicleCommandSender(
        std::string host = "127.0.0.1",
        uint16_t port = 5001,
        std::chrono::milliseconds send_period = std::chrono::milliseconds(50)
    );
    ~VehicleCommandSender();

    VehicleCommandSender(const VehicleCommandSender&) = delete;
    VehicleCommandSender& operator=(const VehicleCommandSender&) = delete;

    void start();
    void stop();

    void set_command(VehicleCommand command);
    void set_command(float acceleration, float steering_angle_velocity);
    void send_stop();

    VehicleCommand command() const;
    bool is_running() const noexcept;
    bool is_connected() const noexcept;
    uint64_t sent_packet_count() const noexcept;
    uint64_t send_failure_count() const noexcept;

private:
    std::string m_host;
    uint16_t m_port = 0;
    std::chrono::milliseconds m_send_period;

    mutable std::mutex m_command_mutex;
    VehicleCommand m_command;

    std::mutex m_wait_mutex;
    std::condition_variable m_wait_condition;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_connected{false};
    std::atomic<uint64_t> m_sent_packet_count{0};
    std::atomic<uint64_t> m_send_failure_count{0};
    std::thread m_thread;

    void send_loop();
    int connect_to_receiver() const;
    bool send_command(int socket, const VehicleCommand& command) const;
};
