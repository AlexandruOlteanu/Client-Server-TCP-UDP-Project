#include <bits/stdc++.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "error.h"

#define CLIENT_NR_ARGS 4
#define AUTOMATED_PROTOCOL 0
#define TAKEN_PORTS 1024
#define MAX_SIZE 100

using namespace std;

/**
 * Se verifica daca am primit comanda exit de la tastatura, in caz afirmativ, se returneaza false (Nu se mai 
 * indeplineste conditia ca acest client sa fie activ)
 * */
bool check_exit_command(string message) {

    string exit_message = "exit";
    for (int i = 0; i < (int) exit_message.size(); ++i) {
        if (message[i] != exit_message[i]) {
            return true;
        }
    }
    return false;
}

/**
 * Functie ce verifica daca comanda curenta este de subscribe sau unsubscribe, 
 * caz in care o trimite catre server prin socketul de tcp pe care serverul asculta 
 * mesajele
 * */
void check_command(int32_t socketfd_tcp, string message, string command_message) {
    bool ok = 1;
    for (int i = 0; i < (int) command_message.size(); ++i) {
        if (message[i] != command_message[i]) {
            ok = 0;
        }
    }
    if (ok) {
        char *buffer = (char *)malloc((int) message.size() * sizeof(char));
        memset(buffer, 0, message.size());
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

/**
 * Functie ce Inchide clientul curent
 * */
void shutdown_subscriber(int32_t socketfd_tcp) {
    shutdown(socketfd_tcp, 2);
}


/**
 * Functie ce transforma un sir de caractere intr-un numar
 * */
int string_to_nr(string message) {
    int32_t number = 0;
    for (auto digit : message) {
        number = number * 10 + (digit - '0');
    }
    return number;
}

int main(int argc, char *argv[]) {

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    // Verificam nuamrul de argumente
    ERROR(argc != CLIENT_NR_ARGS, "Error number of parameters!");

    // Initializam socket-ul de tcp cu -1 dupa care apelam functia 
    // de socket corespunzatoare pentru a deschide conexiunea pe acest file descriptor
    int32_t socketfd_tcp = -1;
    socketfd_tcp = socket(PF_INET, SOCK_STREAM, AUTOMATED_PROTOCOL);
    ERROR(socketfd_tcp == -1, "Error creating tcp socket!");

    // Aflam portul serverului si il stocam intr-o variabila de tip int
    int32_t server_port = string_to_nr(argv[3]);

    // Daca portul nu poate fi atribuit, avem eroare
    ERROR(server_port <= TAKEN_PORTS, "Error, server already taken by main services!");

    // Declaram structuri pentru retinerea adresei serverului
    sockaddr_in *server_adress = (sockaddr_in *)malloc(sizeof(sockaddr_in));
    ERROR(server_adress == NULL, "Error, memory for server adress not allocated!");

    // Initializam adresa serverului cu datele deja extrase si corespunzatoare conform informatiilor din
    // laborator
    memset(server_adress, 0, sizeof(sockaddr_in));
    server_adress->sin_port = htons(server_port);
    int32_t check_ret = inet_aton(argv[2], &(*server_adress).sin_addr);
    server_adress->sin_family = AF_INET;
    
    // Trimitem cererea de conectare de pe socket-ul tocmai deschis la adresa serverului initial.
    // Acesta va trebui ulterior sa accepte conexiunea
    check_ret = connect(socketfd_tcp, (sockaddr *)server_adress, sizeof(sockaddr));
    ERROR(check_ret < 0, "Error, connecting tcp socket failed!");

    // Vom trimite pe acest socket id-ul clientului pentru a verifica conectarea acestuia anterioara sau nu
    check_ret = send(socketfd_tcp, argv[1], strlen(argv[1]), 0);
    ERROR(check_ret < 0, "Error, sending tcp id failed!");

    // Obtinem maximul pentru a putea itera prin "id-ul" de identificare al socketilor
    int32_t maximum_fd = socketfd_tcp;

    // Vom dezactiva algoritmul Neagle cu optiunea de TCP_NODELAY pentru o performanta mai
    // rifdicata
    int disable_neagle = 1;
    check_ret = setsockopt(socketfd_tcp, IPPROTO_TCP, TCP_NODELAY, &disable_neagle, sizeof(int32_t));
    ERROR(check_ret < 0, "Error, disable neagle algorithm");

    bool main_condition = true;

    // Inceperea loop-ului infinit in care clientul asteapta date
    while (main_condition) {
        // Creem un set temporar de file descriptori care mai apoi este modificat de functia 
        // select ce lasa in set doar file descriptorii prezenti in momentul respectiv
        fd_set temporary_fds;
        FD_ZERO(&temporary_fds);
        FD_SET(STDIN_FILENO, &temporary_fds);
		FD_SET(socketfd_tcp, &temporary_fds);
        check_ret = select(maximum_fd + 1, &temporary_fds, NULL, NULL, NULL);
        ERROR(check_ret < 0, "Error, select failed!");

        for (int i = 0; i <= maximum_fd; ++i) {
            // Daca input-ul este de la tastatura, se va verifica daca este comanda de exit, altfel, 
            // daca este de subscribe sau unsubscribe (caz in care se vor rezolva trimiterea mesajelor sparata)
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

            // Daca am primit un mesaj din exterior, acesta este de la server si va fi fie de
            // inchidere, fie un mesaj procesat de udp.
            if (FD_ISSET(i, &temporary_fds)) {
                char *buffer = (char *)malloc(MAX_SIZE * sizeof(char));
                memset(buffer, 0, MAX_SIZE);
                int32_t j = 0;
                check_ret = 1;
                // Pentru a nu pierde date vom realiza citirea byte cu byte si ne folosim de
                // delimitatorul "\n" pentru marcarea incheierii unui mesaj.
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
                string message = buffer;
                // Daca mesajul primit este de close, inchidem serverul, altfel afisam mesajul primit
                if (message == "Close\n") {
                    main_condition = 0;
                    break;
                }
                else {
                    cout << message;
                }
            }
        }
    }

    // Se realizeaza inchiderea serverului
    shutdown_subscriber(socketfd_tcp);

    return 0;
}