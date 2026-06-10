#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/imu.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/msg/point_field.hpp"
#include "geometry_msgs/msg/point_stamped.hpp"
#include "tf2/LinearMath/Matrix3x3.h"
#include "tf2/LinearMath/Quaternion.h"

using std::placeholders::_1;

class PointCloudSocketSender : public rclcpp::Node {
public:
    PointCloudSocketSender()
        : Node("point_cloud_socket_sender") {
        receiver_host_ = declare_parameter<std::string>("receiver_host", "127.0.0.1");
        receiver_port_ = declare_parameter<int>("receiver_port", 5000);
        lidar_topic_ = declare_parameter<std::string>("lidar_topic", "/vehicle/Velodyne_VLP_16/point_cloud");
        gps_topic_ = declare_parameter<std::string>("gps_topic", "/gps");
        imu_topic_ = declare_parameter<std::string>("imu_topic", "/imu");

        lidar_subscription_ = create_subscription<sensor_msgs::msg::PointCloud2>(
            lidar_topic_, rclcpp::SensorDataQoS(), std::bind(&PointCloudSocketSender::lidar_callback, this, _1));

        gps_sub_ = create_subscription<geometry_msgs::msg::PointStamped>(
            gps_topic_, rclcpp::SensorDataQoS(), std::bind(&PointCloudSocketSender::gps_callback, this, _1));

        imu_sub_ = create_subscription<sensor_msgs::msg::Imu>(
            imu_topic_, rclcpp::SensorDataQoS(), std::bind(&PointCloudSocketSender::imu_callback, this, _1));
    }

    ~PointCloudSocketSender() override {
        disconnect();
    }

private:
    static const sensor_msgs::msg::PointField* find_field(
        const sensor_msgs::msg::PointCloud2& msg,
        const std::string& name) {
        for (const auto& field : msg.fields) {
            if (field.name == name) {
                return &field;
            }
        }

        return nullptr;
    }

    template <typename T>
    static T read_scalar_le(const uint8_t* p) {
        T value{};
        std::memcpy(&value, p, sizeof(T));
        return value;
    }

    static bool is_finite3(float x, float y, float z) {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }

    static double lerp(double a, double b, double t) {
        return a + (b - a) * t;
    }

    static double clamp01(double v) {
        return std::clamp(v, 0.0, 1.0);
    }

    void gps_callback(const geometry_msgs::msg::PointStamped::SharedPtr msg) {
        std::lock_guard<std::mutex> lock(gps_mtx_);
        gps_buf_.push_back(*msg);
        trim_buffer(gps_buf_);
    }

    void imu_callback(const sensor_msgs::msg::Imu::SharedPtr msg) {
        std::lock_guard<std::mutex> lock(imu_mtx_);
        imu_buf_.push_back(*msg);
        trim_buffer(imu_buf_);
    }

    template <class MsgT>
    void trim_buffer(std::deque<MsgT>& buffer) {
        if (buffer.empty()) {
            return;
        }

        const rclcpp::Time newest(buffer.back().header.stamp);
        while (!buffer.empty()) {
            const rclcpp::Time oldest(buffer.front().header.stamp);
            if ((newest - oldest) <= max_buf_age_) {
                break;
            }

            buffer.pop_front();
        }
    }

    template <class MsgT>
    bool get_closest_by_stamp(
        const std::deque<MsgT>& buffer,
        const rclcpp::Time& t,
        MsgT& out,
        double* out_dt_sec = nullptr) const {
        if (buffer.empty()) {
            return false;
        }

        size_t best_i = 0;
        double best_dt = std::numeric_limits<double>::infinity();

        for (size_t i = 0; i < buffer.size(); ++i) {
            const rclcpp::Time ti(buffer[i].header.stamp, t.get_clock_type());
            const double dt = std::abs((t - ti).seconds());

            if (dt < best_dt) {
                best_dt = dt;
                best_i = i;
            }
        }

        out = buffer[best_i];

        if (out_dt_sec) {
            *out_dt_sec = best_dt;
        }

        return true;
    }

    bool interp_gps_by_stamp(
        const std::deque<geometry_msgs::msg::PointStamped>& buffer,
        const rclcpp::Time& t,
        geometry_msgs::msg::PointStamped& out,
        double* out_dt_sec = nullptr) const {
        if (buffer.empty()) {
            return false;
        }

        const int64_t t_ns = t.nanoseconds();
        bool have_left = false;
        bool have_right = false;
        size_t i_left = 0;
        size_t i_right = 0;
        int64_t left_ns = 0;
        int64_t right_ns = 0;

        for (size_t i = 0; i < buffer.size(); ++i) {
            const rclcpp::Time ti(buffer[i].header.stamp, t.get_clock_type());
            const int64_t ti_ns = ti.nanoseconds();

            if (ti_ns <= t_ns && (!have_left || ti_ns > left_ns)) {
                have_left = true;
                left_ns = ti_ns;
                i_left = i;
            }

            if (ti_ns >= t_ns && (!have_right || ti_ns < right_ns)) {
                have_right = true;
                right_ns = ti_ns;
                i_right = i;
            }
        }

        if (!have_left || !have_right || left_ns == right_ns) {
            return get_closest_by_stamp(buffer, t, out, out_dt_sec);
        }

        const double alpha = double(t_ns - left_ns) / double(right_ns - left_ns);
        const auto& left = buffer[i_left];
        const auto& right = buffer[i_right];

        out = left;
        out.header.stamp = t;
        out.point.x = lerp(left.point.x, right.point.x, alpha);
        out.point.y = lerp(left.point.y, right.point.y, alpha);
        out.point.z = lerp(left.point.z, right.point.z, alpha);

        if (out_dt_sec) {
            const int64_t dt = std::min(t_ns - left_ns, right_ns - t_ns);
            *out_dt_sec = double(dt) * 1e-9;
        }

        return true;
    }

    bool interp_imu_by_stamp(
        const std::deque<sensor_msgs::msg::Imu>& buffer,
        const rclcpp::Time& t,
        sensor_msgs::msg::Imu& out,
        double* out_dt_sec = nullptr) const {
        if (buffer.empty()) {
            return false;
        }

        const int64_t t_ns = t.nanoseconds();
        bool have_left = false;
        bool have_right = false;
        size_t i_left = 0;
        size_t i_right = 0;
        int64_t left_ns = 0;
        int64_t right_ns = 0;

        for (size_t i = 0; i < buffer.size(); ++i) {
            const rclcpp::Time ti(buffer[i].header.stamp, t.get_clock_type());
            const int64_t ti_ns = ti.nanoseconds();

            if (ti_ns <= t_ns && (!have_left || ti_ns > left_ns)) {
                have_left = true;
                left_ns = ti_ns;
                i_left = i;
            }

            if (ti_ns >= t_ns && (!have_right || ti_ns < right_ns)) {
                have_right = true;
                right_ns = ti_ns;
                i_right = i;
            }
        }

        if (!have_left || !have_right || left_ns == right_ns) {
            return get_closest_by_stamp(buffer, t, out, out_dt_sec);
        }

        const double alpha = clamp01(double(t_ns - left_ns) / double(right_ns - left_ns));
        const auto& left = buffer[i_left];
        const auto& right = buffer[i_right];

        tf2::Quaternion q_left(
            left.orientation.x,
            left.orientation.y,
            left.orientation.z,
            left.orientation.w);
        tf2::Quaternion q_right(
            right.orientation.x,
            right.orientation.y,
            right.orientation.z,
            right.orientation.w);

        q_left.normalize();
        q_right.normalize();

        if (q_left.dot(q_right) < 0.0) {
            q_right = tf2::Quaternion(-q_right.x(), -q_right.y(), -q_right.z(), -q_right.w());
        }

        tf2::Quaternion q_interp = q_left.slerp(q_right, alpha);
        q_interp.normalize();

        out = left;
        out.header.stamp = t;
        out.orientation.x = q_interp.x();
        out.orientation.y = q_interp.y();
        out.orientation.z = q_interp.z();
        out.orientation.w = q_interp.w();

        if (out_dt_sec) {
            const int64_t dt = std::min(t_ns - left_ns, right_ns - t_ns);
            *out_dt_sec = double(dt) * 1e-9;
        }

        return true;
    }

    bool ensure_connected() {
        if (socket_ >= 0) {
            return true;
        }

        socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_ < 0) {
            RCLCPP_ERROR(get_logger(), "socket() failed");
            return false;
        }

        int yes = 1;
        setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(static_cast<uint16_t>(receiver_port_));

        if (inet_pton(AF_INET, receiver_host_.c_str(), &address.sin_addr) <= 0) {
            RCLCPP_ERROR(get_logger(), "Bad receiver_host: %s", receiver_host_.c_str());
            disconnect();
            return false;
        }

        if (connect(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
            RCLCPP_WARN_THROTTLE(
                get_logger(),
                *get_clock(),
                2000,
                "Could not connect to LiDAR receiver at %s:%d",
                receiver_host_.c_str(),
                receiver_port_);
            disconnect();
            return false;
        }

        RCLCPP_INFO(get_logger(), "Connected to LiDAR receiver at %s:%d", receiver_host_.c_str(), receiver_port_);
        return true;
    }

    void disconnect() {
        if (socket_ >= 0) {
            close(socket_);
            socket_ = -1;
        }
    }

    bool send_all(const void* data, size_t byte_count) {
        const auto* bytes = static_cast<const uint8_t*>(data);
        size_t bytes_sent = 0;

        while (bytes_sent < byte_count) {
            const ssize_t n = send(socket_, bytes + bytes_sent, byte_count - bytes_sent, MSG_NOSIGNAL);

            if (n <= 0) {
                disconnect();
                return false;
            }

            bytes_sent += static_cast<size_t>(n);
        }

        return true;
    }

    void append_float(std::vector<uint8_t>& frame, float value) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
        frame.insert(frame.end(), bytes, bytes + sizeof(value));
    }

    void lidar_callback(const sensor_msgs::msg::PointCloud2::ConstSharedPtr point_cloud) {
        const auto* fx = find_field(*point_cloud, "x");
        const auto* fy = find_field(*point_cloud, "y");
        const auto* fz = find_field(*point_cloud, "z");
        const auto* ftime = find_field(*point_cloud, "time");

        if (!fx || !fy || !fz) {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "Point cloud is missing x/y/z fields");
            return;
        }

        {
            std::lock_guard<std::mutex> gps_lock(gps_mtx_);
            std::lock_guard<std::mutex> imu_lock(imu_mtx_);

            if (gps_buf_.empty() || imu_buf_.empty()) {
                RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "Waiting for GPS and IMU samples");
                return;
            }
        }

        const uint32_t point_count = point_cloud->width * point_cloud->height;
        const uint64_t timestamp_ns =
            uint64_t(point_cloud->header.stamp.sec) * 1'000'000'000ull +
            uint64_t(point_cloud->header.stamp.nanosec);

        std::vector<uint8_t> frame;
        frame.reserve(sizeof(timestamp_ns) + sizeof(point_count) + size_t(point_count) * 10 * sizeof(float));

        const uint8_t* timestamp_bytes = reinterpret_cast<const uint8_t*>(&timestamp_ns);
        frame.insert(frame.end(), timestamp_bytes, timestamp_bytes + sizeof(timestamp_ns));

        const uint8_t* count_bytes = reinterpret_cast<const uint8_t*>(&point_count);
        frame.insert(frame.end(), count_bytes, count_bytes + sizeof(point_count));

        const float inf = std::numeric_limits<float>::infinity();
        float min_time = std::numeric_limits<float>::infinity();
        float max_time = 0.0f;

        for (uint32_t i = 0; i < point_count; ++i) {
            const uint8_t* base = point_cloud->data.data() + size_t(i) * point_cloud->point_step;

            float x = read_scalar_le<float>(base + fx->offset);
            float y = read_scalar_le<float>(base + fy->offset);
            float z = read_scalar_le<float>(base + fz->offset);
            float time = ftime ? read_scalar_le<float>(base + ftime->offset) : read_scalar_le<float>(base + 16);

            const rclcpp::Time point_time =
                rclcpp::Time(point_cloud->header.stamp) + rclcpp::Duration::from_seconds(time);

            geometry_msgs::msg::PointStamped gps;
            sensor_msgs::msg::Imu imu;
            {
                std::lock_guard<std::mutex> lock(gps_mtx_);
                if (!interp_gps_by_stamp(gps_buf_, point_time, gps)) {
                    get_closest_by_stamp(gps_buf_, point_time, gps);
                }
            }

            {
                std::lock_guard<std::mutex> lock(imu_mtx_);
                if (!interp_imu_by_stamp(imu_buf_, point_time, imu)) {
                    get_closest_by_stamp(imu_buf_, point_time, imu);
                }
            }

            tf2::Quaternion q_tf(
                imu.orientation.x,
                imu.orientation.y,
                imu.orientation.z,
                imu.orientation.w);

            if (q_tf.length2() <= 0.0) {
                q_tf = tf2::Quaternion(0.0, 0.0, 0.0, 1.0);
            }

            q_tf.normalize();

            double roll = 0.0;
            double pitch = 0.0;
            double yaw = 0.0;
            tf2::Matrix3x3(q_tf).getRPY(roll, pitch, yaw);

            if (time < min_time) {
                min_time = time;
            }

            if (time > max_time) {
                max_time = time;
            }

            if (!is_finite3(x, y, z)) {
                x = inf;
                y = inf;
                z = inf;
            }

            append_float(frame, x);
            append_float(frame, y);
            append_float(frame, z);
            append_float(frame, time);
            append_float(frame, static_cast<float>(gps.point.x));
            append_float(frame, static_cast<float>(gps.point.y));
            append_float(frame, static_cast<float>(gps.point.z));
            append_float(frame, static_cast<float>(roll));
            append_float(frame, static_cast<float>(pitch));
            append_float(frame, static_cast<float>(yaw));
        }

        if (!ensure_connected()) {
            return;
        }

        if (!send_all(frame.data(), frame.size())) {
            RCLCPP_WARN(get_logger(), "Failed to send LiDAR frame; will reconnect on the next frame");
            return;
        }

        RCLCPP_INFO_THROTTLE(
            get_logger(),
            *get_clock(),
            1000,
            "Sent frame %lu: %u points, %.2f MB, point time %.6f..%.6f",
            frame_id_,
            point_count,
            static_cast<double>(frame.size()) / (1024.0 * 1024.0),
            min_time,
            max_time);

        ++frame_id_;
    }

    std::string receiver_host_;
    int receiver_port_ = 5000;
    std::string lidar_topic_;
    std::string gps_topic_;
    std::string imu_topic_;
    int socket_ = -1;
    uint64_t frame_id_ = 0;

    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr lidar_subscription_;
    rclcpp::Subscription<geometry_msgs::msg::PointStamped>::SharedPtr gps_sub_;
    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_sub_;

    std::mutex imu_mtx_;
    std::mutex gps_mtx_;
    std::deque<geometry_msgs::msg::PointStamped> gps_buf_;
    std::deque<sensor_msgs::msg::Imu> imu_buf_;
    rclcpp::Duration max_buf_age_ = rclcpp::Duration::from_seconds(2.0);
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<PointCloudSocketSender>());
    rclcpp::shutdown();
    return 0;
}
