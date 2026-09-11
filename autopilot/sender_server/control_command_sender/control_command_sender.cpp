#include "control_command_sender.h"

ControlCommandSender::ControlCommandSender(
    std::string receiver_host, 
    uint16_t receiver_port)
    :   SenderServer<ControlCommand>(receiver_host, receiver_port) {}