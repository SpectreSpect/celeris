#pragma once

#include "../receiver_server.h"

class SteeringAngleReceiver : public ReceiverServer<float> {
public:
    _XCLASS_NAME(SteeringAngleReceiver);

    SteeringAngleReceiver(
        uint16_t port = 5006,
        size_t max_queued_messages = 3
    );

    ~SteeringAngleReceiver() override;

    bool read_exact_message(int client_socket, float& message) override;
};
