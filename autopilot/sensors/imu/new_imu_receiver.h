#pragma once

#include "../receiver_server.h"
#include "imu_measurement.h"

class NewImuReceiver : public ReceiverServer<ImuMeasurement> {
public:
    _XCHILD_NAME(NewImuReceiver);

    NewImuReceiver(
        uint16_t port = 5003,
        size_t max_queued_imu_messages = 1
    );

    ~NewImuReceiver() override;

    bool try_pop_back_imu_message(ImuMeasurement& message);
    bool read_exact_message(int client_socket, ImuMeasurement& message) override;
};
