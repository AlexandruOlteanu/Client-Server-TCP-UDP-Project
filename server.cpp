#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "error.h"
using namespace std;

#define SERVER_NR_ARGS 2
#define AUTOMATED_PROTOCOL 0
#define TAKEN_PORTS 1024
#define MAX_SIZE 100
#define MAX_SUBSCRIBERS 1000


struct subscriber_info {
    string id_client;
    string ip_server;
    int server_port;
};

struct database {
    multiset<string> id_subscribers;
    multiset<subscriber_info> subscribers;
    map<int32_t, subscriber_info> connected_subscribers;

};


database server_database; 

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

void send_close_message_subscriber(int32_t closing_socket) {
    int check_res = send(closing_socket, "Close", 5, 0);
    ERROR(check_res < 0, "Error, Closing message failed");
}

void shutdown_server(int32_t &socketfd_udp, int32_t &socketfd_tcp) {
    cout << "Am ajuns aici\n";
    for (auto u : server_database.connected_subscribers) {
        send_close_message_subscriber(u.first);
    }
    server_database.connected_subscribers.clear();

    shutdown(socketfd_udp, 2);
    shutdown(socketfd_tcp, 2);
}

void process_subscriber(sockaddr_in &subscriber, int32_t socket_tcp) {
    subscriber_info current_subscriber_info;
    char *id = (char *)malloc(MAX_SIZE * sizeof(char));
    memset(id, 0, MAX_SIZE);
    int32_t check_ret = recv(socket_tcp, id, 50, 0);
    ERROR(check_ret < 0, "Error, recv failed!");
    current_subscriber_info.id_client = id;
    char s[200];
    inet_ntop(AF_INET, &(subscriber.sin_addr), s, 16);
    current_subscriber_info.ip_server = s;
    current_subscriber_info.server_port = subscriber.sin_port;
    if (server_database.id_subscribers.find(current_subscriber_info.id_client) == server_database.id_subscribers.end()) {
        cout << "New client " << current_subscriber_info.id_client << " connected from " << 
        current_subscriber_info.ip_server << ":" << current_subscriber_info.server_port <<".\n";
        server_database.id_subscribers.insert(current_subscriber_info.id_client);
        server_database.connected_subscribers.insert({socket_tcp, current_subscriber_info});
    }
    else {
        cout << "Client " << current_subscriber_info.id_client << " already connected.\n";
    }
}   


int main(int argc, char *argv[]) {

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    ERROR(argc < SERVER_NR_ARGS, "Error number of parameters!");

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
    sockaddr_in *subscriber_address = (sockaddr_in *)malloc(sizeof(sockaddr_in));
    ERROR(server_adress == NULL, "Error, memory for server adress not allocated!");

    memset(server_adress, 0, sizeof(server_adress));
    server_adress->sin_port = htons(server_port);
    server_adress->sin_addr.s_addr = INADDR_ANY;
    server_adress->sin_family = AF_INET;

    int32_t check_ret = bind(socketfd_udp, (sockaddr *)server_adress, sizeof(sockaddr));
    ERROR(check_ret < 0, "Error, binding udp socket failed!");

    check_ret = bind(socketfd_tcp, (sockaddr *)server_adress, sizeof(sockaddr));
    ERROR(check_ret < 0, "Error, binding tcp socket failed!");

    check_ret = listen(socketfd_tcp, MAX_SUBSCRIBERS);
    ERROR(check_ret < 0, "Error, listen from socket failed!");

    fd_set read_fds, temporary_fds;
    FD_ZERO(&read_fds);
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
            if (i == STDIN_FILENO && FD_ISSET(STDIN_FILENO, &temporary_fds)) {
                main_condition &= check_exit_command();
                if (!main_condition) {
                    break;
                }
                continue;
            }

            if (FD_ISSET(socketfd_udp, &temporary_fds) && i == socketfd_udp) {
                
                

                continue;
            }

            if (FD_ISSET(socketfd_tcp, &temporary_fds) && i == socketfd_tcp) {
                socklen_t new_socketfd_tcp_size = sizeof(sockaddr_in);
                int32_t new_socketfd_tcp = accept(socketfd_tcp, (sockaddr *)subscriber_address, (socklen_t *) &new_socketfd_tcp_size); 
                ERROR(new_socketfd_tcp < 0, "Error, accepting connection failed");
                
                FD_SET(new_socketfd_tcp, &read_fds);
                maximum_fd = max(maximum_fd, new_socketfd_tcp);

                process_subscriber(*subscriber_address, new_socketfd_tcp);

                continue;
            }

            if (FD_ISSET(i, &temporary_fds)) {
                char message[MAX_SIZE];
                check_ret = recv(i, message, sizeof(message), 0);
                ERROR(check_ret < 0, "Error, recieving message failed");

                if (!check_ret) {
                    subscriber_info disconnected_subscriber = server_database.connected_subscribers[i];
                    server_database.connected_subscribers.erase(i);
                    server_database.id_subscribers.erase(disconnected_subscriber.id_client);
                    cout << "Client " << disconnected_subscriber.id_client << " disconnected.\n";
                    close(i);
                    FD_CLR(i, &read_fds);
                }
            }

        }
    }

    shutdown_server(socketfd_udp, socketfd_tcp);

    return 0;
}