#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define TCP_PORT 8080
#define UDP_PORT 9090
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define MAX_CAMPUS_NAME 50

typedef struct {
    int socket;
    char campus_name[MAX_CAMPUS_NAME];
    struct sockaddr_in udp_address;
    time_t last_heartbeat;
    int active;
} Client;

typedef struct {
    char campus[MAX_CAMPUS_NAME];
    char password[MAX_CAMPUS_NAME];
} Credential;

Credential valid_credentials[] = {
    {"Islamabad", "NU-ISB-123"},
    {"Lahore", "NU-LHR-123"},
    {"Karachi", "NU-KHI-123"},
    {"Peshawar", "NU-PSH-123"},
    {"CFD", "NU-CFD-123"},
    {"Multan", "NU-MLT-123"}
};
int num_credentials = 6;

Client clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

int tcp_socket_fd, udp_socket_fd;

void init_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = -1;
        clients[i].active = 0;
        memset(clients[i].campus_name, 0, MAX_CAMPUS_NAME);
    }
}

int authenticate_client(char* campus, char* password) {
    for (int i = 0; i < num_credentials; i++)
        if (strcmp(valid_credentials[i].campus, campus) == 0 &&
            strcmp(valid_credentials[i].password, password) == 0)
            return 1;
    return 0;
}

int find_client_by_name(char* campus_name) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].active && strcmp(clients[i].campus_name, campus_name) == 0)
            return i;
    return -1;
}

void log_message(char* message) {
    time_t now = time(NULL);
    char* time_str = ctime(&now);
    time_str[strlen(time_str)-1] = '\0';
    printf("[%s] %s\n", time_str, message);
}

void broadcast_udp(char* message) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].active && clients[i].udp_address.sin_port != 0)
            sendto(udp_socket_fd, message, strlen(message), 0,
                   (struct sockaddr*)&clients[i].udp_address, sizeof(clients[i].udp_address));
    pthread_mutex_unlock(&clients_mutex);
}

void* handle_client(void* arg) {
    int client_socket = *((int*)arg); free(arg);
    char buffer[BUFFER_SIZE], campus_name[MAX_CAMPUS_NAME] = {0};
    int authenticated = 0, client_index = -1;

    int recv_len = recv(client_socket, buffer, BUFFER_SIZE-1, 0);
    if (recv_len <= 0) { close(client_socket); return NULL; }

    buffer[recv_len] = '\0';
    if (strncmp(buffer, "AUTH:", 5) == 0) {
        char* campus = strtok(buffer+5, ":");
        char* password = strtok(NULL, ":");
        if (campus && password && authenticate_client(campus, password)) {
            strcpy(campus_name, campus);
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (!clients[i].active) {
                    clients[i].socket = client_socket;
                    strcpy(clients[i].campus_name, campus_name);
                    clients[i].active = 1;
                    clients[i].last_heartbeat = time(NULL);
                    memset(&clients[i].udp_address, 0, sizeof(struct sockaddr_in));
                    client_index = i;
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            send(client_socket, "AUTH_OK", 7, 0);
            authenticated = 1;
            char log_msg[BUFFER_SIZE]; sprintf(log_msg, "[AUTH] %s campus authenticated", campus_name);
            log_message(log_msg);
        } else {
            send(client_socket, "AUTH_FAIL", 9, 0);
            close(client_socket);
            return NULL;
        }
    }

    while (authenticated) {
        memset(buffer, 0, BUFFER_SIZE);
        recv_len = recv(client_socket, buffer, BUFFER_SIZE-1, 0);
        if (recv_len <= 0) break;
        buffer[recv_len] = '\0';

        if (strncmp(buffer, "MSG:", 4) == 0) {
            char* msg_copy = strdup(buffer);
            char* source_campus = strtok(buffer+4, ":");
            char* source_dept = strtok(NULL, ":");
            char* target_campus = strtok(NULL, ":");
            char* target_dept = strtok(NULL, ":");
            char* content = strtok(NULL, "");

            if (target_campus && content) {
                pthread_mutex_lock(&clients_mutex);
                int target_index = find_client_by_name(target_campus);
                if (target_index >= 0 && clients[target_index].active) {
                    send(clients[target_index].socket, msg_copy, strlen(msg_copy), 0);
                } else {
                    char error_msg[BUFFER_SIZE];
                    sprintf(error_msg, "ERROR:Campus %s is not online", target_campus);
                    send(client_socket, error_msg, strlen(error_msg), 0);
                }
                pthread_mutex_unlock(&clients_mutex);
            }
            free(msg_copy);
        }
    }

    pthread_mutex_lock(&clients_mutex);
    if (client_index >= 0) {
        clients[client_index].active = 0;
        clients[client_index].socket = -1;
        char log_msg[BUFFER_SIZE]; sprintf(log_msg, "[DISCONNECT] %s campus disconnected", campus_name);
        log_message(log_msg);
    }
    pthread_mutex_unlock(&clients_mutex);
    close(client_socket);
    return NULL;
}

void* udp_handler(void* arg) {
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int recv_len = recvfrom(udp_socket_fd, buffer, BUFFER_SIZE-1, 0,
                                (struct sockaddr*)&client_addr, &addr_len);
        if (recv_len > 0) {
            buffer[recv_len] = '\0';
            if (strncmp(buffer, "HEARTBEAT:", 10) == 0) {
                char* campus_name = buffer + 10;
                pthread_mutex_lock(&clients_mutex);
                int idx = find_client_by_name(campus_name);
                if (idx >= 0) {
                    clients[idx].last_heartbeat = time(NULL);
                    memcpy(&clients[idx].udp_address, &client_addr, sizeof(struct sockaddr_in));
                }
                pthread_mutex_unlock(&clients_mutex);
            }
        }
    }
    return NULL;
}

void* admin_console(void* arg) {
    char command[BUFFER_SIZE];
    while (1) {
        printf("admin> "); fflush(stdout);
        if (fgets(command, BUFFER_SIZE, stdin) == NULL) continue;
        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "status") == 0) {
            pthread_mutex_lock(&clients_mutex);
            time_t now = time(NULL);
            for (int i = 0; i < MAX_CLIENTS; i++)
                if (clients[i].active)
                    printf("%s - Last heartbeat: %lds ago\n",
                           clients[i].campus_name, now - clients[i].last_heartbeat);
            pthread_mutex_unlock(&clients_mutex);
        } else if (strncmp(command, "broadcast ", 10) == 0) {
            char* msg = command + 10;
            char full_msg[BUFFER_SIZE];
            sprintf(full_msg, "BROADCAST:%s", msg);
            broadcast_udp(full_msg);
        } else if (strcmp(command, "help") == 0) {
            printf("status, broadcast <msg>, help\n");
        } else if (strlen(command) > 0) printf("Unknown command\n");
    }
    return NULL;
}

int main() {
    struct sockaddr_in tcp_addr, udp_addr;
    pthread_t udp_thread, admin_thread;

    init_clients();

    tcp_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket_fd < 0) { perror("TCP socket creation failed"); exit(EXIT_FAILURE); }

    int opt = 1;
    setsockopt(tcp_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&tcp_addr, 0, sizeof(tcp_addr));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(TCP_PORT);

    if (bind(tcp_socket_fd, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr)) < 0) { perror("TCP bind failed"); exit(EXIT_FAILURE); }
    if (listen(tcp_socket_fd, MAX_CLIENTS) < 0) { perror("TCP listen failed"); exit(EXIT_FAILURE); }

    udp_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket_fd < 0) { perror("UDP socket creation failed"); exit(EXIT_FAILURE); }

    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(UDP_PORT);

    if (bind(udp_socket_fd, (struct sockaddr*)&udp_addr, sizeof(udp_addr)) < 0) { perror("UDP bind failed"); exit(EXIT_FAILURE); }

    pthread_create(&udp_thread, NULL, udp_handler, NULL);
    pthread_create(&admin_thread, NULL, admin_console, NULL);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_socket = accept(tcp_socket_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_socket < 0) { perror("Accept failed"); continue; }

        pthread_t thread_id;
        int* client_sock_ptr = malloc(sizeof(int));
        *client_sock_ptr = client_socket;
        if (pthread_create(&thread_id, NULL, handle_client, client_sock_ptr) != 0) {
            perror("Thread creation failed"); close(client_socket); free(client_sock_ptr);
        }
        pthread_detach(thread_id);
    }

    close(tcp_socket_fd);
    close(udp_socket_fd);
    return 0;
}
