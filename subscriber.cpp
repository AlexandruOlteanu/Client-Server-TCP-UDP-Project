#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "error.h"

#define CLIENT_NR_ARGS 4
#define AUTOMATED_PROTOCOL 0
#define TAKEN_PORTS 1024
#define MAX_SIZE 100

using namespace std;

bool check_exit_command(string message) {

    string exit_message = "exit";
    for (int i = 0; i < (int) exit_message.size(); ++i) {
        if (message[i] != exit_message[i]) {
            return true;
        }
    }
    return false;
}

void check_command(int32_t socketfd_tcp, string message, string command_message) {
    bool ok = 1;
    for (int i = 0; i < (int) command_message.size(); ++i) {
        if (message[i] != command_message[i]) {
            ok = 0;
        }
    }
    if (ok) {
        char *buffer = (char *)malloc((int) message.size() * sizeof(char));
        memset(buffer, 0, sizeof(buffer));
        strcpy(buffer, message.c_str());
        int32_t check_ret = send(socketfd_tcp, buffer, strlen(buffer), 0);
        ERROR(check_ret < 0, "Error, sending tcp message failed");

        if (command_message == "subscribe") {
            cout << "Subscribed to topic.\n";
        }

        if (command_message == "unsubscribe") {
            cout << "Unsubscribed from topic.\n";
        }
    }
    return;
}

void shutdown_subscriber(int32_t socketfd_tcp) {
    shutdown(socketfd_tcp, 2);
}


int main(int argc, char *argv[]) {

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    ERROR(argc < CLIENT_NR_ARGS, "Error number of parameters!");

    int32_t socketfd_tcp = -1;
    socketfd_tcp = socket(PF_INET, SOCK_STREAM, AUTOMATED_PROTOCOL);
    ERROR(socketfd_tcp == -1, "Error creating tcp socket!");

    int32_t server_port = 0;
    std :: string string_port = argv[3];
    for (auto digit : string_port) {
        server_port = server_port * 10 + (digit - '0');
    }

    ERROR(server_port <= TAKEN_PORTS, "Error, server already taken by main services!");

    sockaddr_in *server_adress = (sockaddr_in *)malloc(sizeof(sockaddr_in));
    ERROR(server_adress == NULL, "Error, memory for server adress not allocated!");

    memset(server_adress, 0, sizeof(server_adress));
    server_adress->sin_port = htons(server_port);
    int32_t check_ret = inet_aton(argv[2], &(*server_adress).sin_addr);
    server_adress->sin_family = AF_INET;
    
    check_ret = connect(socketfd_tcp, (sockaddr *)server_adress, sizeof(sockaddr));
    ERROR(check_ret < 0, "Error, connecting tcp socket failed!");

    check_ret = send(socketfd_tcp, argv[1], strlen(argv[1]), 0);
    ERROR(check_ret < 0, "Error, sending tcp id failed!");

    int32_t maximum_fd = socketfd_tcp;

    bool main_condition = true;

    while (main_condition) {
        
        fd_set temporary_fds;
        FD_ZERO(&temporary_fds);
        FD_SET(STDIN_FILENO, &temporary_fds);
		FD_SET(socketfd_tcp, &temporary_fds);
        check_ret = select(maximum_fd + 1, &temporary_fds, NULL, NULL, NULL);
        ERROR(check_ret < 0, "Error, select failed!");

        for (int i = 0; i <= maximum_fd; ++i) {
            if (i == STDIN_FILENO && FD_ISSET(STDIN_FILENO, &temporary_fds)) {
                string message;
                getline(cin, message);
                main_condition &= check_exit_command(message);
                if (!main_condition) {
                    break;
                }
                check_command(socketfd_tcp, message, "subscribe");
                check_command(socketfd_tcp, message, "unsubscribe");
                continue;
            }

            if (FD_ISSET(i, &temporary_fds)) {
                char *buffer = (char *)malloc(MAX_SIZE * sizeof(char));
                memset(buffer, 0, sizeof(buffer));
                int32_t j = 0;
                check_ret = 1;
                while (true) {
                    char c = '#';
                    check_ret = recv(i, &c, 1, 0);
                    ERROR(check_ret != 1, "Error, failed to read one byte");
                    buffer[j] = c;
                    if (c == '\n') {
                        break;
                    }
                    ++j;
                }
                cout << buffer;
            }
        }
    }

    shutdown_subscriber(socketfd_tcp);

    return 0;
}