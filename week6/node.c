#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <memory.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "alist.h"

#define NETWORK_IP_ADDRESS "192.168.0.0"
#define NETWORK_PORT 2000

#define LIST_REQUEST "GET_LIST\n"
#define PING "PING"
#define PONG "PONG"

#define PING_SIZE 4
#define MAX_LIST_SIZE 1024

p_array_list nodes;

struct node {
    char name[32];
    char ip[16];
    char port[16];
};

void * udp_receive_ping(void *arg) {
    char ping_buffer[PING_SIZE];
    int ping_recv_socket = 0, addr_len = 0;
    struct sockaddr_in server_addr, client_addr;

    ping_recv_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    addr_len = sizeof(struct sockaddr);
    memset(&server_addr, 0, addr_len);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NETWORK_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(ping_recv_socket, (struct sockaddr *) &server_addr, addr_len);

    while (1) {
        memset(ping_buffer, 0, PING_SIZE);
        memset(&client_addr, 0, addr_len);

        recvfrom(ping_recv_socket, (void *)ping_buffer, PING_SIZE, MSG_WAITALL,
            (struct sockaddr *) &client_addr, &addr_len);

        if (strncmp(PING, ping_buffer, PING_SIZE) == 0) {
            sendto(ping_recv_socket, PONG, PING_SIZE, MSG_CONFIRM,
		        (struct sockaddr *) &client_addr, addr_len);
        }
    }
}

void * udp_send_ping(void *arg) {
    char ping_buffer[PING_SIZE];
    int sockfd = 0, addr_len = 0;
    struct sockaddr_in dest;
    struct hostent *host;

    addr_len = sizeof(struct sockaddr);

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    while(1) {
	struct node *current;
        int i = array_list_iter(nodes);

        for (; i != -1; i = array_list_next(nodes, i)) {
            current = array_list_get(nodes, i);

            host = (struct hostent *)gethostbyname(current->ip);
            dest.sin_family = AF_INET;
            dest.sin_port = htons(atoi(current->ip));
            dest.sin_addr = *((struct in_addr *)host->h_addr);

	        printf("PING %s:%s:%u", current->name, inet_ntoa(dest.sin_addr), ntohs(dest.sin_port));

	        sendto(sockfd, PING, PING_SIZE, MSG_CONFIRM,
		        (struct sockaddr *)&dest, addr_len);

            printf(" - PING SENT - ");

	        recvfrom(sockfd, ping_buffer, PING_SIZE, MSG_WAITALL,
                (struct sockaddr *)&dest, &addr_len);

            if (strncmp(PONG, ping_buffer, PING_SIZE) == 0) {
                printf("PONG RECEIVED\n");
            }
        }
    }
}

void new_node_info(struct node *new_node) {
    int sock_fd = 0, addr_len = 0;
    struct sockaddr_in dest;
    struct hostent *host;
    char message[48];
    struct node *current;

    int i, k, j = 0;
    for (k = 0; new_node->name[k] != '\0'; j++, k++) {
		message[j] = new_node->name[k];
	}
    message[j++] = ':';
    for (k = 0; new_node->ip[k] != '\0'; j++, k++) {
		message[j] = new_node->ip[k];
	}
    message[j++] = ':';
    for (k = 0; new_node->port[k] != '\0'; j++, k++) {
		message[j] = new_node->port[k];
	}
    message[j++] = '\n';
    message[j++] = '.';
    message[j] = '\n';

    addr_len = sizeof(struct sockaddr);

    sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    i = array_list_iter(nodes), j = 0;
    for (; i != -1; i = array_list_next(nodes, i)) {
        current = array_list_get(nodes, i);

        host = (struct hostent *)gethostbyname(current->ip);
        dest.sin_family = AF_INET;
        dest.sin_port = htons(atoi(current->ip));
        dest.sin_addr = *((struct in_addr *)host->h_addr);

        connect(sock_fd, (struct sockaddr *)&dest, addr_len);

	sendto(sock_fd, message, sizeof(message), 0,
            (struct sockaddr *)&dest, addr_len);

        close(sock_fd);
    }
}

void tcp_request_network_connection() {
    int sock_fd = 0, addr_len = 0;
    struct sockaddr_in dest;
    struct hostent *host;
    char name[32];
    char message[48];

    printf("Your Network name: ");
    scanf("%s", name);

    int i = 0, j = 0;
    for (; i < strlen(LIST_REQUEST); i++) {
        message[i] = LIST_REQUEST[i];
    }
    for (; j < strlen(name); i++, j++) {
        message[i] = name[j];
    }
    message[i++] = '\n';
    message[i++] = '.';
    message[i] = '\n';

    addr_len = sizeof(struct sockaddr);

    host = (struct hostent *)gethostbyname(NETWORK_IP_ADDRESS);
    dest.sin_family = AF_INET;
    dest.sin_port = NETWORK_PORT;
    dest.sin_addr = *((struct in_addr *)host->h_addr);

    sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    connect(sock_fd, (struct sockaddr *)&dest, addr_len);

	sendto(sock_fd, message, sizeof(message), 0,
		    (struct sockaddr *)&dest, addr_len);

    char* received_data = (char*)malloc(MAX_LIST_SIZE);

	recvfrom(sock_fd, (char *)&received_data, MAX_LIST_SIZE, 0,
		    (struct sockaddr *)&dest, &addr_len);

    int start = 0, end = 0;
    for (; end < MAX_LIST_SIZE; end++) {
        if (received_data[end] == '\n') {
            struct node* new_node = (struct node *)malloc(sizeof(struct node));

            int i = 0;
            while (received_data[start] != ':') {
                new_node->name[i++] = received_data[start++];
            }
            i = 0;
            start++;
            while (received_data[start] != ':') {
                new_node->ip[i++] = received_data[start++];
            }
            i = 0;
            start++;
            while (start != end) {
                new_node->port[i++] = received_data[start++];
            }

            array_list_add(nodes, new_node);
            start = end + 1;
        }
        if (strncmp(received_data + end, "\n.\n", 3) == 0) {
            break;
        }
    }

    free(received_data);
    close(sock_fd);
}

void tcp_listen() {
    char data_buffer[1024];
    int master_sock_fd = 0, comm_sock_fd = 0, addr_len = 0;
    struct sockaddr_in server_addr, client_addr;
    fd_set readfds;

    addr_len = sizeof(struct sockaddr);

    master_sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = NETWORK_PORT;
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(master_sock_fd, (struct sockaddr *) &server_addr, addr_len);

    listen(master_sock_fd, 5);

    while (1) {
        memset(data_buffer, 0, sizeof(data_buffer));
        FD_ZERO(&readfds);
        FD_SET(master_sock_fd, &readfds);

        select(master_sock_fd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(master_sock_fd, &readfds)) {
            comm_sock_fd = accept(master_sock_fd, (struct sockaddr *) &client_addr, &addr_len);

            while (1) {
                recvfrom(comm_sock_fd, (char *) data_buffer, sizeof(data_buffer), 0,
                    (struct sockaddr *) &client_addr, &addr_len);

                if (strncmp(data_buffer, LIST_REQUEST, strlen(LIST_REQUEST)) == 0) {
                    char name[32];
                    int i = strlen(LIST_REQUEST) + 1, j = 0, k;
                    while (data_buffer[i] != '\n') {
                        name[j++] = data_buffer[i++];
                    }

                    // Send list
                    struct node *current;
                    i = array_list_iter(nodes), j = 0;
                    memset(data_buffer, 0, sizeof(data_buffer));

                    for (; i != -1; i = array_list_next(nodes, i)) {
                        current = array_list_get(nodes, i);

                        for (k = 0; current->name[k] != '\0'; j++, k++) {
		                    data_buffer[j] =  current->name[k];
	                    }
                        data_buffer[j++] = ':';
                        for (k = 0; current->ip[k] != '\0'; j++, k++) {
		                    data_buffer[j] =  current->ip[k];
	                    }
                        data_buffer[j++] = ':';
                        for (k = 0; current->port[k] != '\0'; j++, k++) {
		                    data_buffer[j] =  current->port[k];
	                    }
                        data_buffer[j++] = '\n';
                    }

                    data_buffer[j++] = '.';
                    data_buffer[j] = '\n';

                    sendto(comm_sock_fd, data_buffer, sizeof(data_buffer), 0,
                        (struct sockaddr *) &client_addr, addr_len);

                    struct node *new_node = (struct node *)malloc(sizeof(struct node));
		    for (; name[i] != '\0'; i++) {
			new_node->name[i] = name[i];
		    }
		    char *ip = inet_ntoa(client_addr.sin_addr);
		    for (; ip[i] != '\0'; i++) {
			new_node->ip[i] = ip[i];
		    }
		    unsigned int port = ntohs(client_addr.sin_port);
		    sprintf(new_node->port, "%u", port);

                    new_node_info(new_node);
                    array_list_add(nodes, new_node);
                } else {
                    // Add new node
                    int start = 0, end = 0, i = 0;
                    while (data_buffer[end] != '\n') { end++; }

                    struct node *new_node = (struct node *)malloc(sizeof(struct node));
                    while (data_buffer[start] != ':') {
                        new_node->name[i++] = data_buffer[start++];
                    }
                    i = 0;
                    start++;
                    while (data_buffer[start] != ':') {
                        new_node->ip[i++] = data_buffer[start++];
                    }
                    i = 0;
                    start++;
                    while (start != end) {
                        new_node->port[i++] = data_buffer[start++];
                    }

                    array_list_add(nodes, new_node);
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    nodes = create_array_list(16);
    pthread_t udp_send_ping_thread;
    pthread_t udp_receive_ping_thread;

    // Ask for list of connected nodes
    tcp_request_network_connection();

    // Create PING/PONG loops
    pthread_create(&udp_send_ping_thread, NULL, udp_send_ping, NULL);
    pthread_create(&udp_receive_ping_thread, NULL, udp_receive_ping, NULL);

    // Listen for new nodes info
    tcp_listen();
    return 0;
}
