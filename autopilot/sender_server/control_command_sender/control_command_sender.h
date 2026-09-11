#pragma once

#include "../sender_server.h"
#include "control_command.h"

class ControlCommandSender : public SenderServer<ControlCommand> {
public:
    ControlCommandSender(
        std::string receiver_host = "127.0.0.1", 
        uint16_t receiver_port = 2005
    );
};