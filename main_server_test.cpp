// server.cpp
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "socket failed\n";
        return 1;
    }

    int yes = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(5000);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "bind failed\n";
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 1) < 0) {
        std::cerr << "listen failed\n";
        close(server_fd);
        return 1;
    }

    std::cout << "Waiting on port 5000...\n";

    int client_fd = accept(server_fd, nullptr, nullptr);
    if (client_fd < 0) {
        std::cerr << "accept failed\n";
        close(server_fd);
        return 1;
    }

    char buffer[1024];

    while (true) {
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);

        if (bytes_read == 0) {
            std::cout << "Client disconnected\n";
            break;
        }

        if (bytes_read < 0) {
            std::cerr << "recv failed\n";
            break;
        }

        std::cout << "Received " << bytes_read << " bytes: ";
        std::cout.write(buffer, bytes_read);
        std::cout << "\n";
    }

    close(client_fd);
    close(server_fd);
}