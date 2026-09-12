#include "steering_angle_receiver.h"

SteeringAngleReceiver::SteeringAngleReceiver(
    uint16_t port,
    size_t max_queued_messages)
    : ReceiverServer<float>(port, max_queued_messages) {
}

SteeringAngleReceiver::~SteeringAngleReceiver() {
    stop();
}

bool SteeringAngleReceiver::read_exact_message(int client_socket, float& message) {
    return read_exact(client_socket, &message, sizeof(message));
}
