#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

struct VehicleCommand {
    float speed = 0.0f;
    float steering_angle = 0.0f;
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
    void set_command(float speed, float steering_angle);
    void send_stop();

    VehicleCommand command() const;
    bool is_running() const noexcept;
    bool is_connected() const noexcept;

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
    std::thread m_thread;

    void send_loop();
    int connect_to_receiver() const;
    bool send_command(int socket, const VehicleCommand& command) const;
};
