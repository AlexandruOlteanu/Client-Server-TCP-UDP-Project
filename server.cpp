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
using namespace std;

#define SERVER_NR_ARGS 2
#define AUTOMATED_PROTOCOL 0
#define TAKEN_PORTS 1024
#define MAX_SIZE 100
#define MAX_SUBSCRIBERS 1000

/** Structura ce salveaza date despre un subscriber (client)
 *  De asemenea, aici se retin topic-urile la care un client este abonat
 *  + acele topicuri la care este abonat cu sore forward enabled
 * */
struct subscriber_info {
    string id_client;
    string ip_server;
    int32_t server_port;
    int32_t socket_fd;
    multiset<string> subscribed_topics;
    multiset<string> subscribed_sf_1;
};

/**
 * Pentru fiecare topic vom avea un vector de subscriberi care sunt abonati la acest topic
 * */
struct topic_data {
    vector<subscriber_info> subscribers;
};

/**
 * Database-ul principal, aici se retine id-ul clientilor conectati in prezent, 
 * o mapa care retine legatura intre socket-ul de pe care a venit conexiunea 
 * unui subscriber si subscriber-ul in sine, acestia fiind cei conectati in 
 * prezent. De asemenea, avem si o mapa ce creaza o legatura intre denumirile
 * topicurilor si vectorul de subscriberi abonati la acestea.
 * */
struct database {
    multiset<string> id_subscribers;
    map<int32_t, subscriber_info> connected_subscribers;
    map<string, topic_data> topic_subscribers;
};

/**
 * Structura ce ajuta la retinerea si parsarea datelor primite dintr-un 
 * mesaj trimis de clientul UDP. Aceasta retine denumirea topicului, 
 * tipul de date trimis in continuare si payload-ul (sau content-ul efectiv)
 * De asemenea avem si ip-ul, respectiv portul de unde a fost trimis mesajul.
 * */
struct recieved_udp_data {
    char topic[50];
    uint8_t data_type;
    char message[1500];
    string ip_udp; 
    int32_t udp_port;
};

/**
 * Vom retine mesajele ce trebuie trimise pentru diferiti clienti care au
 * fost deconectati si au store forward-ul setat pe 1 ca 
 * o mapa in care cheia este id-ul clientului iar valoarea este un dequeue unde
 * adaugam mesajele la final si cand trimitem le traversam de la inceput
 * */
unordered_map<string, deque<string>> waiting_messages;

string nr_to_string(int32_t number);

/**
 * Functionalitate pentru extragerea corespunzatoare a unui mesaj din datele
 * trimise de un client udp
 * */
struct extract_message {

    /**
     * Construim mesajul de baza ce este alcatuit din ip-ul de unde au fost 
     * trimise datele, portul si topicul corespunzator
     * */
    string build_udp_message(recieved_udp_data udp_data) {
        string message_to_send = udp_data.ip_udp + ":" + nr_to_string(udp_data.udp_port) + " - " + udp_data.topic + " - ";
        return message_to_send;
    }

    /**
     * Daca tipul de date este INT, se extrage corespunzator bitul de semn si apoi numarul care este trecut
     * din network byte order in host byte order. Astfel, se verifica daca numele este negativ sau pozitiv 
     * si se creaza mesajul corespunzator
     * */
    string extract_int(recieved_udp_data udp_data) {
        string message_to_send = build_udp_message(udp_data);
        message_to_send += "INT - ";
        int32_t int_nr;
        memcpy(&int_nr, udp_data.message + 1, sizeof(int32_t));
        int_nr = ntohl(int_nr);
        message_to_send += (udp_data.message[0] == 1 ? ("-" + nr_to_string(int_nr)) : nr_to_string(int_nr));
        message_to_send += "\n";
        return message_to_send;
    }

    /**
     * Daca tipul de date este INT, se extrage corespunzator numarul care este trecut
     * din network byte order in host byte order. Apoi, mesajul este construit prin concatenarea partii intregi cu virgula
     * si ultimele 2 zecimale. Am realizat acest lucru folosind functia de substring.
     * */
    string extract_short_real(recieved_udp_data udp_data) {
        string message_to_send = build_udp_message(udp_data);
        message_to_send += "SHORT_REAL - ";
        uint16_t short_nr;
        memcpy(&short_nr, udp_data.message, sizeof(uint16_t));
        short_nr = ntohs(short_nr);
        string float_result = nr_to_string(short_nr);
        float_result = float_result.substr(0, float_result.size() - 2) + "." + float_result.substr(float_result.size() - 2, 2);
        message_to_send += float_result;
        message_to_send += "\n";
        return message_to_send;
    }

    /**
     * Se extrage semnul numarului, numarul dorit si puterea lui 10 la care acesta urmeaza sa fie imparit. Apoi, se imparte numarul
     * dorit la aceasta putere si se transforma in string. In final, eliminam zecimalele nedorite care sunt egale cu 0 de la sfarsit 
     * */
    string extract_float(recieved_udp_data udp_data) {
        string message_to_send = build_udp_message(udp_data);
        message_to_send += "FLOAT - ";
        bool sign = 0;
        if (udp_data.message[0] == 1) {
            sign = 1;
        }
        uint32_t number;
        memcpy(&number, udp_data.message + 1, sizeof(uint32_t));
        uint8_t power;
        memcpy(&power, udp_data.message + 1 + sizeof(uint32_t), sizeof(uint8_t));
        number = ntohl(number);
        double result = number / (double) (pow(10, power));
        result *= (pow(10, power));
        result = (int)result;
        result /= (pow(10, power));
        string res = to_string(result);
        while (res[res.size() - 1] == '0') {
            res.erase(res.size() - 1);
        } 
        message_to_send += (sign ? ("-" + res) : res);
        message_to_send += "\n";
        return message_to_send;
    }

    /**
     * Se extrage sirul din payload si se adauga la mesaj. 
     * */
    string extract_string(recieved_udp_data udp_data) {
        string message_to_send = build_udp_message(udp_data);
        message_to_send += "STRING - ";
        message_to_send += udp_data.message;
        message_to_send += "\n";
        return message_to_send;
    }
};

/**
 * Functie ce primeste informatiile unui client si trimite mesajele care 
 * au fost trimise cat timp acesta a fost deconectat insa a avut store forward-ul
 * setat pe 1. Se parcurge intregul dequeue si se trimit toate mesajele
 * */
void send_waiting_messages(subscriber_info subscriber) {

    int check_ret = 1;
    while (!waiting_messages[subscriber.id_client].empty()) {
        string message_to_send = waiting_messages[subscriber.id_client].front();
        check_ret = send(subscriber.socket_fd, message_to_send.c_str(), message_to_send.size(), 0);
        ERROR(check_ret < 0,  "Error, sending udp message\n");
        waiting_messages[subscriber.id_client].pop_front();
    }

}

database server_database; 

/**
 * Se verifica daca am primit comanda exit de la tastatura, in caz afirmativ, se returneaza false (Nu se mai 
 * indeplineste conditia ca serverul sa fie activ)
 * */
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

/**
 * Functie ce inchide un client prin trimiterea mesajului de Close pe socket-ul respectiv 
 * acestuia
 * */
void send_close_message_subscriber(int32_t closing_socket) {
    int check_res = send(closing_socket, "Close\n", 6, 0);
    ERROR(check_res < 0, "Error, Closing message failed");
}

/**
 * Functie ce faciliteaza inchiderea serverului care se realizeaza prin inchiderea initiala 
 * a tuturor clientilor conectati si apoi inchiderea serverului.
 * */
void shutdown_server(int32_t &socketfd_udp, int32_t &socketfd_tcp) {
    for (auto u : server_database.connected_subscribers) {
        send_close_message_subscriber(u.first);
    }
    server_database.connected_subscribers.clear();

    shutdown(socketfd_udp, 2);
    shutdown(socketfd_tcp, 2);
}

/**
 * Functie prin care se primeste o noua conexiune de la un subscriber, identificandu-l prin 
 * adresa socket-ului (in care se afla port-ul si ip-ul) si socketul in sine.
 * Se primeste apoi id-ul cu ajutorul functiei recv, se salveaza datele noului posibil subscriber 
 * si apoi se verifica daca este deja conectat sau nu. In cazul in care nu este, se adauga 
 * la lista de clienti conectati si se afiseaza mesajul de informare, altfel se afiseaza mesajul 
 * de eroare "Client <ID> already connected" si se trimite un mesaj de inchidere al respectivului 
 * nou deschis client. De asemenea, in momentul in care un client a revenit online, se trimit toate
 * mesajele care asteptau pe topicurile la care acesta era abonat cu sf = 1
 * */
void process_subscriber(sockaddr_in &subscriber, int32_t socket_tcp) {
    subscriber_info current_subscriber_info;
    char *id = (char *)malloc(MAX_SIZE * sizeof(char));
    memset(id, 0, MAX_SIZE);
    int32_t check_ret = recv(socket_tcp, id, 50, 0);
    ERROR(check_ret < 0, "Error, recv failed!");
    current_subscriber_info.id_client = id;
    current_subscriber_info.socket_fd = socket_tcp;
    current_subscriber_info.ip_server = inet_ntoa(subscriber.sin_addr);
    current_subscriber_info.server_port = subscriber.sin_port;
    if (server_database.id_subscribers.find(current_subscriber_info.id_client) == server_database.id_subscribers.end()) {
        cout << "New client " << current_subscriber_info.id_client << " connected from " << 
        current_subscriber_info.ip_server << ":" << current_subscriber_info.server_port <<".\n";
        server_database.id_subscribers.insert(current_subscriber_info.id_client);
        server_database.connected_subscribers.insert({socket_tcp, current_subscriber_info});
        send_waiting_messages(current_subscriber_info);
    }
    else {
        cout << "Client " << current_subscriber_info.id_client << " already connected.\n";
        send_close_message_subscriber(current_subscriber_info.socket_fd);
    }
}   
/**
 * Functie ce transforma un numar intreg intr-un sir de caractere
 * */
string nr_to_string(int32_t number) {
    string message = "";
    while (number) {
        message = char(number % 10 + '0') + message;
        number /= 10;
    }
    return message;
}
/**
 * Functie care transforma un sir de caractere intr-un numar
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
    ERROR(argc != SERVER_NR_ARGS, "Error number of parameters!");

    // Initializam socket-ul de tcp si udp cu -1 dupa care apelam functia 
    // de socket corespunzatoare pentru a primi date
    int32_t socketfd_udp = -1, socketfd_tcp = -1;

    socketfd_udp = socket(PF_INET, SOCK_DGRAM, AUTOMATED_PROTOCOL);
    ERROR(socketfd_udp == -1, "Error creating udp socket!");

    socketfd_tcp = socket(PF_INET, SOCK_STREAM, AUTOMATED_PROTOCOL);
    ERROR(socketfd_tcp == -1, "Error creating tcp socket!");

    // Aflam portul serverului si il stocam intr-o variabila de tip int
    int32_t server_port = string_to_nr(argv[1]);

    // Daca portul nu poate fi atribuit, avem eroare
    ERROR(server_port <= TAKEN_PORTS, "Error, server already taken by main services!");

    // Declaram structuri pentru retinerea adresei serverului si viitoarele adrese ale subscriber-ilor
    // ce se vor conecta in viitor
    sockaddr_in *server_adress = (sockaddr_in *)malloc(sizeof(sockaddr_in));
    ERROR(server_adress == NULL, "Error, memory for server adress not allocated!");
    sockaddr_in *subscriber_address = (sockaddr_in *)malloc(sizeof(sockaddr_in));
    ERROR(subscriber_address == NULL, "Error, memory for subscriber not allocated");

    // Initializam adresa serverului cu datele deja extrase si corespunzatoare conform informatiilor din
    // laborator
    memset(server_adress, 0, sizeof(sockaddr_in));
    server_adress->sin_port = htons(server_port);
    server_adress->sin_addr.s_addr = INADDR_ANY;
    server_adress->sin_family = AF_INET;

    // Facem legatura intre cei doi socketi si adresa serverului
    int32_t check_ret = bind(socketfd_udp, (sockaddr *)server_adress, sizeof(sockaddr));
    ERROR(check_ret < 0, "Error, binding udp socket failed!");

    check_ret = bind(socketfd_tcp, (sockaddr *)server_adress, sizeof(sockaddr));
    ERROR(check_ret < 0, "Error, binding tcp socket failed!");

    // Vom asculta input-uri de mesaje de pe socketul de tcp, astfel vom 
    // putea primi mesaje de la TCP
    check_ret = listen(socketfd_tcp, MAX_SUBSCRIBERS);
    ERROR(check_ret < 0, "Error, listen from socket failed!");

    // Declaram si initializam lista de file descriptori. Adaugam toate posibilitatile de 
    // file descriptori in set (Input de la TCP, UDP si tastatura)
    fd_set read_fds, temporary_fds;
    FD_ZERO(&read_fds);
    FD_SET(socketfd_udp, &read_fds);
    FD_SET(socketfd_tcp, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    
    // Obtinem maximul pentru a putea itera prin "id-ul" de identificare al socketilor
    int32_t maximum_fd = max(socketfd_udp, socketfd_tcp);

    // Vom dezactiva algoritmul Neagle cu optiunea de TCP_NODELAY pentru o performanta mai
    // rifdicata
    int32_t disable_neagle = 1;
    check_ret = setsockopt(socketfd_tcp, IPPROTO_TCP, TCP_NODELAY, &disable_neagle, sizeof(int32_t));
    ERROR(check_ret < 0, "Error, disable neagle algorithm");

    // Inceperea loop-ului infinit in care serverul asteapta date
    bool main_condition = true;
    while (main_condition) {
        // Creem un set temporar de file descriptori care mai apoi este modificat de functia 
        // select ce lasa in set doar file descriptorii prezenti in momentul respectiv
        temporary_fds = read_fds;
        check_ret = select(maximum_fd + 1, &temporary_fds, NULL, NULL, NULL);
        ERROR(check_ret < 0, "Error, select failed!");

        for (int i = 0; i <= maximum_fd; ++i) {
            // Daca avem input de la STDIN, vom verifica aparitia comenzii exit, caz in care vom inchide serverul
            if (i == STDIN_FILENO && FD_ISSET(STDIN_FILENO, &temporary_fds)) {
                main_condition &= check_exit_command();
                if (!main_condition) {
                    break;
                }
                continue;
            }
            // Vom primi mesaje de la UDP
            if (FD_ISSET(socketfd_udp, &temporary_fds) && i == socketfd_udp) {
                // Se citesc date din socketul de udp si se salveaza atat mesajul trimis cat si 
                // adresa de unde a venit conexiunea. Astfel, vom avea datele necesare pentru a 
                // realiza afisarea corecta a datelor (folosind port-ul si adresa ip a transmitatorului)
                char *message = (char *)malloc(2000 * sizeof(char));
                ERROR(message == NULL, "Error, memory for message was not allocated");
                memset(message, 0, MAX_SIZE);
                uint32_t addr_len = sizeof(sockaddr_in);
                sockaddr_in *udp_sender = (sockaddr_in *)malloc(sizeof(sockaddr_in));
                check_ret = recvfrom(socketfd_udp, message, MAX_SIZE, 0, (struct sockaddr *) udp_sender, &addr_len);
                ERROR(check_ret < 0, "Error, recieving data from udp");
                
                // Copiem mesajul in forma sa bruta corespunzator in structura de date udp.
                // Astfel, se atribuie corespunzator valorile celor trei campuri necesare.
                // Dupa care, se preiau portul si adrsa ip
                recieved_udp_data udp_data;
                memcpy((void *)&udp_data, message, sizeof(recieved_udp_data));
                udp_data.ip_udp = inet_ntoa(udp_sender->sin_addr);
                udp_data.udp_port = udp_sender->sin_port;
                string port_string = nr_to_string(udp_data.udp_port);

                // Se va realliza extragerea mesajului necesar in functie de tipul de date cu ajutorul functiilor
                // declarate anterior
                extract_message extract;
                string message_to_send;
                if ((int32_t) udp_data.data_type == 0) {
                    message_to_send = extract.extract_int(udp_data);
                } 
                else if ((int32_t) udp_data.data_type == 1) {
                    message_to_send = extract.extract_short_real(udp_data);
                } 
                else if ((int32_t) udp_data.data_type == 2) {
                    message_to_send = extract.extract_float(udp_data);
                } 
                else if ((int32_t) udp_data.data_type == 3) {
                    message_to_send = extract.extract_string(udp_data);
                }

                // Vom parcurge lista de subscriberi a topicului primit in prezent. Daca subscriber-ul
                // este conectat se trimite mesajul catre el, altfel, daca acesta are store forward setat pe 1, 
                // se va pune mesajul in lista acestuia de asteptare pentru cand se va reconecta
                for (auto u : server_database.topic_subscribers[udp_data.topic].subscribers) {
                    if (server_database.connected_subscribers.find(u.socket_fd) != server_database.connected_subscribers.end()) {
                        check_ret = send(u.socket_fd, message_to_send.c_str(), message_to_send.size(), 0);
                        ERROR(check_ret < 0, "Error, failed to send message");
                    }
                    else {
                        if (u.subscribed_sf_1.find(udp_data.topic) != u.subscribed_sf_1.end()) {
                            waiting_messages[u.id_client].push_back(message_to_send);
                        }
                    }
                }

                continue;
            }
            // Procesam o noua conexiune de la un client tcp
            if (FD_ISSET(socketfd_tcp, &temporary_fds) && i == socketfd_tcp) {
                // Acceptam aceasta conexiune ce asteapta sa primeasca un raspuns si trimitem mai departe
                // socketul si adresa de unde aceasta conexiune a venit
                uint32_t addr_len = sizeof(sockaddr_in);
                int32_t tcp_sender = accept(socketfd_tcp, (sockaddr *)subscriber_address, (socklen_t *) &addr_len); 
                ERROR(tcp_sender < 0, "Error, accepting connection failed");
                
                // Adaugam noul socket la setul de file descriptori pentru a fi procesat ulterior
                FD_SET(tcp_sender, &read_fds);
                maximum_fd = max(maximum_fd, tcp_sender);

                // Procesam noul subscriber dupa metodele prezentate mai sus
                process_subscriber(*subscriber_address, tcp_sender);

                continue;
            }
            
            // Cazul in care primim un mesaj de la un client insa acesta nu este de conexiune noua
            if (FD_ISSET(i, &temporary_fds)) {
                // Interceptam mesajul acestuia
                char message[MAX_SIZE];
                memset(message, 0, sizeof(message));
                check_ret = recv(i, message, sizeof(message), 0);
                ERROR(check_ret < 0, "Error, recieving message failed");
                
                // Daca mesajul este gol, inseamna ca respectivul client este inchis, moment in care il declaram
                // ca fiind deconectat si il eliminam din listele corespunzatoare
                if (!check_ret) {
                    if (server_database.connected_subscribers.find(i) != server_database.connected_subscribers.end()) {
                        subscriber_info disconnected_subscriber = server_database.connected_subscribers[i];
                        server_database.connected_subscribers.erase(i);
                        server_database.id_subscribers.erase(disconnected_subscriber.id_client);
                        cout << "Client " << disconnected_subscriber.id_client << " disconnected.\n";
                    }
                    close(i);
                    FD_CLR(i, &read_fds);
                    continue;
                }
                // Verificam daca acesta a trimis o comanda de subscribe pentru un topic
                string command = "subscribe";
                int sz = strlen(message);
                for (int i = 0; i < sz; ++i) {
                    if (message[i] == ' ') {
                        message[i] = '\0';
                    }
                }
                bool equal = true;
                for (int i = 0; i < (int32_t) command.size() && equal; ++i) {
                    if (command[i] != message[i]) {
                        equal = false;
                        break;
                    }
                }
                // Daca da, vom extrage topicul transmis de acesta si valoarea lui store forward si vom realiza
                // operatiile corespunzatoare
                if (equal) {
                    char topic_name[MAX_SIZE];
                    memset(topic_name, 0, sizeof(topic_name));
                    memcpy(topic_name, message + command.size() + 1, strlen(message + command.size() + 1));
                    char store_forward = 0;
                    memcpy(&store_forward, message + command.size() + 1 + strlen(message + command.size() + 1) + 1, sizeof(char));
                    if (store_forward == '1') {
                        server_database.connected_subscribers[i].subscribed_sf_1.insert(topic_name);
                    }
                    bool not_found = 0;
                    for (auto u : server_database.topic_subscribers[topic_name].subscribers) {
                        if (u.id_client == server_database.connected_subscribers[i].id_client) {
                            not_found = 1;
                            break;
                        }
                    }
                    if (!not_found) {
                        server_database.topic_subscribers[topic_name].subscribers.push_back(server_database.connected_subscribers[i]);
                    }
                }
                // In cazul comenzii de unsubscribe, vom scoate clientul din lista de subscriberi a topicului dorit.
                command = "unsubscribe";
                equal = true;
                for (int i = 0; i < (int32_t) command.size() && equal; ++i) {
                    if (command[i] != message[i]) {
                        equal = false;
                        break;
                    }
                }
                if (equal) {
                    char topic_name[MAX_SIZE];
                    memset(topic_name, 0, sizeof(topic_name));
                    memcpy(topic_name, message + command.size() + 1, strlen(message + command.size() + 1));
                    for (int j = 0; j < (int32_t) server_database.topic_subscribers[topic_name].subscribers.size(); ++j) {
                        if (server_database.topic_subscribers[topic_name].subscribers[j].socket_fd == i) {
                            auto pos = server_database.topic_subscribers[topic_name].subscribers.begin() + j;
                            server_database.topic_subscribers[topic_name].subscribers.erase(pos);
                            break;
                        }
                    }
                }
            }
        }
    }

    // Realizam inchiderea serverului
    shutdown_server(socketfd_udp, socketfd_tcp);

    return 0;
}