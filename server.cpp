#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "error.h"
using namespace std;


#define AUTOMATED_PROTOCOL 0
#define TAKEN_PORTS 1024
#define MAX_SIZE 100


bool check_exit_command() {

    string message = "";
    string exit_message = "exit";
    cin >> message;
    for (int i = 0; i < (int) exit_message.size(); ++i) {
        if (message[i] != exit_message[i]) {
            return true;
        }
    }
    return false;
}

void close_server(int32_t &socketfd_udp, int32_t &socketfd_tcp) {
    close(socketfd_udp);
    close(socketfd_tcp);
}


int main(int argc, char *argv[]) {

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    ERROR(argc < 2, "Error number of parameters!");

    int32_t socketfd_udp = -1, socketfd_tcp = -1;
    
    socketfd_udp = socket(PF_INET, SOCK_DGRAM, AUTOMATED_PROTOCOL);
    ERROR(socketfd_udp == -1, "Error creating udp socket!");

    socketfd_tcp = socket(PF_INET, SOCK_STREAM, AUTOMATED_PROTOCOL);
    ERROR(socketfd_tcp == -1, "Error creating tcp socket!");

    int32_t server_port = 0;
    std :: string string_port = argv[1];
    for (auto digit : string_port) {
        server_port = server_port * 10 + (digit - '0');
    }
    
    ERROR(server_port <= TAKEN_PORTS, "Error, server already taken by main services!");

    sockaddr_in *server_adress = (sockaddr_in *)malloc(sizeof(sockaddr_in));
    ERROR(server_adress == NULL, "Error, memory for server adress not allocated!");

    memset(server_adress, 0, sizeof(server_adress));
    server_adress->sin_port = htons(server_port);
    server_adress->sin_addr.s_addr = INADDR_ANY;
    server_adress->sin_family = AF_INET;

    int32_t check_ret = bind(socketfd_udp, (sockaddr *)server_adress, sizeof(sockaddr));
    ERROR(check_ret < 0, "Error, binding udp socket failed!");

    check_ret = bind(socketfd_tcp, (sockaddr *)server_adress, sizeof(sockaddr));
    ERROR(check_ret < 0, "Error, binding tcp socket failed!");

    fd_set read_fds, temporary_fds;
    FD_SET(socketfd_udp, &read_fds);
    FD_SET(socketfd_tcp, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);

    int32_t maximum_fd = max(socketfd_udp, socketfd_tcp);

    bool main_condition = true;
    while (main_condition) {
        temporary_fds = read_fds;
        check_ret = select(maximum_fd + 1, &temporary_fds, NULL, NULL, NULL);
        ERROR(check_ret < 0, "Error, select failed!");

        for (int i = 0; i <= maximum_fd; ++i) {
            if (FD_ISSET(STDIN_FILENO, &temporary_fds)) {
                main_condition &= check_exit_command();
                if (!main_condition) {
                    break;
                }
                continue;
            }

            if (FD_ISSET(i, &temporary_fds) && i == socketfd_udp) {
                
                continue;
            }

            if (FD_ISSET(i, &temporary_fds) && i == socketfd_tcp) {
                
                continue;
            }

        }
    }

    close_server(socketfd_udp, socketfd_tcp);

    return 0;
}