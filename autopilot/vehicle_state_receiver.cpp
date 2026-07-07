#include "vehicle_state_receiver.h"

#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <bit>
#include <iostream>

namespace {
constexpr size_t feedback_float_count = 18;
constexpr size_t feedback_packet_size =
    sizeof(uint64_t) + sizeof(uint32_t) + feedback_float_count * sizeof(float);

uint32_t read_u32_little_endian(const uint8_t* data) {
    return
        static_cast<uint32_t>(data[0]) |
        (static_cast<uint32_t>(data[1]) << 8u) |
        (static_cast<uint32_t>(data[2]) << 16u) |
        (static_cast<uint32_t>(data[3]) << 24u);
}

uint64_t read_u64_little_endian(const uint8_t* data) {
    uint64_t value = 0;
    for (size_t i = 0; i < sizeof(uint64_t); i++) {
        value |= static_cast<uint64_t>(data[i]) << (8u * i);
    }
    return value;
}

float read_float_little_endian(const uint8_t* data) {
    const uint32_t bits = read_u32_little_endian(data);
    return std::bit_cast<float>(bits);
}

VehicleFeedback parse_feedback_packet(const std::array<uint8_t, feedback_packet_size>& packet) {
    VehicleFeedback feedback;
    const uint8_t* data = packet.data();

    feedback.timestamp_ns = read_u64_little_endian(data);
    data += sizeof(uint64_t);

    feedback.flags = read_u32_little_endian(data);
    data += sizeof(uint32_t);

    std::array<float, feedback_float_count> values{};
    for (float& value : values) {
        value = read_float_little_endian(data);
        data += sizeof(float);
    }

    feedback.position_ros = glm::vec3(values[0], values[1], values[2]);
    feedback.orientation_ros = glm::quat(values[6], values[3], values[4], values[5]);
    feedback.linear_velocity_ros = glm::vec3(values[7], values[8], values[9]);
    feedback.angular_velocity_ros = glm::vec3(values[10], values[11], values[12]);
    feedback.speed = values[13];
    feedback.acceleration = values[14];
    feedback.steering_angle = values[15];
    feedback.steering_angle_velocity = values[16];
    feedback.steering_angle_acceleration = values[17];
    feedback.received_at = std::chrono::steady_clock::now();

    return feedback;
}
}

VehicleStateReceiver::VehicleStateReceiver(uint16_t port)
    : m_port(port) {}

VehicleStateReceiver::~VehicleStateReceiver() {
    stop();
}

void VehicleStateReceiver::start() {
    if (m_running.exchange(true))
        return;

    m_thread = std::thread(&VehicleStateReceiver::receive_loop, this);
}

void VehicleStateReceiver::stop() {
    if (!m_running.exchange(false))
        return;

    close_listen_socket();

    int client_socket = m_client_socket.exchange(-1);
    if (client_socket >= 0)
        close(client_socket);

    if (m_thread.joinable())
        m_thread.join();

    m_connected = false;
}

bool VehicleStateReceiver::latest_feedback(VehicleFeedback& feedback) const {
    std::lock_guard lock(m_feedback_mutex);
    if (!m_has_feedback)
        return false;

    feedback = m_latest_feedback;
    return true;
}

bool VehicleStateReceiver::is_running() const noexcept {
    return m_running.load();
}

bool VehicleStateReceiver::is_connected() const noexcept {
    return m_connected.load();
}

void VehicleStateReceiver::receive_loop() {
    m_listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listen_socket < 0) {
        std::cerr << "VehicleStateReceiver: socket failed\n";
        m_running = false;
        return;
    }

    int yes = 1;
    setsockopt(m_listen_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(m_port);

    if (bind(m_listen_socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "VehicleStateReceiver: bind failed on port " << m_port << "\n";
        close_listen_socket();
        m_running = false;
        return;
    }

    if (listen(m_listen_socket, 1) < 0) {
        std::cerr << "VehicleStateReceiver: listen failed\n";
        close_listen_socket();
        m_running = false;
        return;
    }

    std::cout << "VehicleStateReceiver: listening on port " << m_port << "\n";

    while (m_running.load()) {
        sockaddr_in client_address{};
        socklen_t client_len = sizeof(client_address);

        int client_socket = accept(
            m_listen_socket,
            reinterpret_cast<sockaddr*>(&client_address),
            &client_len
        );

        if (client_socket < 0) {
            if (m_running.load())
                std::cerr << "VehicleStateReceiver: accept failed\n";
            continue;
        }

        std::cout << "VehicleStateReceiver: client connected\n";
        m_client_socket = client_socket;
        m_connected = true;

        receive_feedback_from_client(client_socket);

        if (m_client_socket.exchange(-1) == client_socket)
            close(client_socket);

        m_connected = false;
        std::cout << "VehicleStateReceiver: client disconnected\n";
    }

    close_listen_socket();
}

bool VehicleStateReceiver::receive_feedback_from_client(int client_socket) {
    while (m_running.load()) {
        std::array<uint8_t, feedback_packet_size> packet{};

        if (!read_exact(client_socket, packet.data(), packet.size()))
            return false;

        set_latest_feedback(parse_feedback_packet(packet));
    }

    return true;
}

bool VehicleStateReceiver::read_exact(int socket, void* data, size_t byte_count) {
    auto* bytes = static_cast<uint8_t*>(data);
    size_t bytes_read = 0;

    while (bytes_read < byte_count && m_running.load()) {
        ssize_t n = recv(socket, bytes + bytes_read, byte_count - bytes_read, 0);

        if (n < 0 && errno == EINTR)
            continue;

        if (n <= 0)
            return false;

        bytes_read += static_cast<size_t>(n);
    }

    return bytes_read == byte_count;
}

void VehicleStateReceiver::set_latest_feedback(VehicleFeedback feedback) {
    std::lock_guard lock(m_feedback_mutex);
    m_latest_feedback = feedback;
    m_has_feedback = true;
}

void VehicleStateReceiver::close_listen_socket() {
    if (m_listen_socket >= 0) {
        close(m_listen_socket);
        m_listen_socket = -1;
    }
}
