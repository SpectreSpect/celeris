#include "vehicle_command_sender.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <bit>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <iostream>
#include <utility>

namespace {
constexpr std::chrono::milliseconds reconnect_delay(500);
constexpr int connect_timeout_ms = 250;

void write_float_little_endian(uint8_t* destination, float value) {
    uint32_t bits = std::bit_cast<uint32_t>(value);
    destination[0] = static_cast<uint8_t>(bits);
    destination[1] = static_cast<uint8_t>(bits >> 8u);
    destination[2] = static_cast<uint8_t>(bits >> 16u);
    destination[3] = static_cast<uint8_t>(bits >> 24u);
}

}

VehicleCommandSender::VehicleCommandSender(
    std::string host,
    uint16_t port,
    std::chrono::milliseconds send_period)
    : m_host(std::move(host)),
      m_port(port),
      m_send_period(send_period)
{
    if (m_send_period <= std::chrono::milliseconds::zero())
        m_send_period = std::chrono::milliseconds(50);
}

VehicleCommandSender::~VehicleCommandSender() {
    stop();
}

void VehicleCommandSender::start() {
    if (m_running.exchange(true))
        return;

    m_thread = std::thread(&VehicleCommandSender::send_loop, this);
}

void VehicleCommandSender::stop() {
    if (!m_running.exchange(false))
        return;

    m_wait_condition.notify_all();
    if (m_thread.joinable())
        m_thread.join();

    m_connected = false;
}

void VehicleCommandSender::set_command(VehicleCommand command) {
    if (!std::isfinite(command.speed) || !std::isfinite(command.steering_angle))
        return;

    {
        std::lock_guard lock(m_command_mutex);
        m_command = command;
    }
    m_wait_condition.notify_all();
}

void VehicleCommandSender::set_command(float speed, float steering_angle) {
    set_command(VehicleCommand{
        .speed = speed,
        .steering_angle = steering_angle
    });
}

void VehicleCommandSender::send_stop() {
    set_command(VehicleCommand{});
}

VehicleCommand VehicleCommandSender::command() const {
    std::lock_guard lock(m_command_mutex);
    return m_command;
}

bool VehicleCommandSender::is_running() const noexcept {
    return m_running.load();
}

bool VehicleCommandSender::is_connected() const noexcept {
    return m_connected.load();
}

void VehicleCommandSender::send_loop() {
    while (m_running.load()) {
        int socket = connect_to_receiver();
        if (socket < 0) {
            std::unique_lock lock(m_wait_mutex);
            m_wait_condition.wait_for(lock, reconnect_delay, [this] {
                return !m_running.load();
            });
            continue;
        }

        std::cout << "VehicleCommandSender: connected to "
                  << m_host << ':' << m_port << '\n';
        m_connected = true;

        while (m_running.load()) {
            if (!send_command(socket, command()))
                break;

            std::unique_lock lock(m_wait_mutex);
            m_wait_condition.wait_for(lock, m_send_period, [this] {
                return !m_running.load();
            });
        }

        if (!m_running.load())
            send_command(socket, VehicleCommand{});

        close(socket);
        m_connected = false;

        if (m_running.load())
            std::cerr << "VehicleCommandSender: connection lost; reconnecting\n";
    }
}

int VehicleCommandSender::connect_to_receiver() const {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* addresses = nullptr;
    const std::string port = std::to_string(m_port);
    if (getaddrinfo(m_host.c_str(), port.c_str(), &hints, &addresses) != 0)
        return -1;

    int connected_socket = -1;
    for (addrinfo* address = addresses;
         address && m_running.load();
         address = address->ai_next) {
        int candidate = socket(
            address->ai_family,
            address->ai_socktype,
            address->ai_protocol
        );
        if (candidate < 0)
            continue;

        int flags = fcntl(candidate, F_GETFL, 0);
        if (flags < 0 || fcntl(candidate, F_SETFL, flags | O_NONBLOCK) < 0) {
            close(candidate);
            continue;
        }

        int result = connect(candidate, address->ai_addr, address->ai_addrlen);
        bool connected = result == 0;

        if (!connected && errno == EINPROGRESS) {
            pollfd descriptor{
                .fd = candidate,
                .events = POLLOUT,
                .revents = 0
            };

            if (poll(&descriptor, 1, connect_timeout_ms) > 0) {
                int socket_error = 0;
                socklen_t error_size = sizeof(socket_error);
                connected = getsockopt(
                    candidate,
                    SOL_SOCKET,
                    SO_ERROR,
                    &socket_error,
                    &error_size
                ) == 0 && socket_error == 0;
            }
        }

        if (connected && fcntl(candidate, F_SETFL, flags) == 0) {
            connected_socket = candidate;
            break;
        }

        close(candidate);
    }

    freeaddrinfo(addresses);
    return connected_socket;
}

bool VehicleCommandSender::send_command(
    int socket,
    const VehicleCommand& command) const
{
    // Wire format: two IEEE-754 float32 values in little-endian order:
    // speed followed by steering_angle.
    std::array<uint8_t, 8> packet{};
    write_float_little_endian(packet.data(), command.speed);
    write_float_little_endian(packet.data() + 4, command.steering_angle);

    size_t sent = 0;
    while (sent < packet.size()) {
        ssize_t result = send(
            socket,
            packet.data() + sent,
            packet.size() - sent,
            MSG_NOSIGNAL
        );

        if (result < 0 && errno == EINTR)
            continue;
        if (result <= 0)
            return false;

        sent += static_cast<size_t>(result);
    }

    return true;
}
