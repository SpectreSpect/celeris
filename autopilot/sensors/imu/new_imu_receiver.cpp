#include "new_imu_receiver.h"

NewImuReceiver::NewImuReceiver(uint16_t port, size_t max_queued_imu_messages)
    : ReceiverServer<ImuMeasurement>(
        port, max_queued_imu_messages, QueueOverflowPolicy::DropNewest) {}

NewImuReceiver::~NewImuReceiver() {
    stop();
}

bool NewImuReceiver::try_pop_back_imu_message(ImuMeasurement& message) {
    return try_pop_back(message);
}

bool NewImuReceiver::read_exact_message(int client_socket, ImuMeasurement& message) {
    return read_exact(client_socket, &message, sizeof(ImuMeasurement));
}
