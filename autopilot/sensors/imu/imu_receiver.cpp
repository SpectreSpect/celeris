#include "imu_receiver.h"

ImuReceiver::ImuReceiver(uint16_t port, size_t max_queued_imu_messages)
    : ReceiverServer<ImuMeasurement>(
        port, max_queued_imu_messages, QueueOverflowPolicy::DropNewest) {}

ImuReceiver::~ImuReceiver() {
    stop();
}

bool ImuReceiver::try_pop_back_imu_message(ImuMeasurement& message) {
    return try_pop_back(message);
}

bool ImuReceiver::read_exact_message(int client_socket, ImuMeasurement& message) {
    return read_exact(client_socket, &message, sizeof(ImuMeasurement));
}
