#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "alist.h"

#define NETWORK_IP_ADDRESS "10.244.1.175"
#define NETWORK_PORT 12000
#define PING_PORT 12100

#define LIST_REQUEST "GET_LIST\n"
#define PING "ping"
#define PONG "pong"

#define PING_SIZE 4
#define MAX_LIST_SIZE 1024

p_array_list nodes;
char network_name[32];

struct node {
    char name[32];
    char ip[16];
    char port[16];
};

void * udp_receive_ping(void *arg) {
    char ping_buffer[PING_SIZE + 1];
    int ping_recv_socket = 0, addr_len = 0;
    struct sockaddr_in server_addr, client_addr;

    ping_recv_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    addr_len = sizeof(struct sockaddr_in);
    memset(&server_addr, 0, addr_len);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PING_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(ping_recv_socket, (struct sockaddr *) &server_addr, addr_len);

    while (1) {
        memset(ping_buffer, 0, PING_SIZE + 1);
        memset(&client_addr, 0, addr_len);

        printf("Nothing happened...\n");

        recvfrom(ping_recv_socket, ping_buffer, PING_SIZE + 1, 0,
            (struct sockaddr *) &client_addr, &addr_len);

        printf("Something happened...\n");

        if (strncmp(PING, ping_buffer, PING_SIZE) == 0) {
            sendto(ping_recv_socket, PONG, PING_SIZE + 1, 0,
		        (struct sockaddr *) &client_addr, addr_len);
        }
    }
}

void * udp_send_ping(void *arg) {
    char ping_buffer[PING_SIZE + 1];
    int sockfd = 0, addr_len = 0;
    struct sockaddr_in dest;

    addr_len = sizeof(struct sockaddr_in);

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    while(1) {
        sleep(2);
	    struct node *current;
        int i = array_list_iter(nodes);

        for (; i != -1; i = array_list_next(nodes, i)) {
            current = array_list_get(nodes, i);

            dest.sin_family = AF_INET;
            dest.sin_port = htons(PING_PORT);
            inet_pton(AF_INET, current->ip, &dest.sin_addr);

	        printf("PING %s:%s:%u", current->name, current->ip, PING_PORT);

	        sendto(sockfd, PING, PING_SIZE + 1, 0,
		        (struct sockaddr *)&dest, addr_len);

            printf(" SENT - ");

	        recvfrom(sockfd, ping_buffer, PING_SIZE + 1, 0,
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

        dest.sin_family = AF_INET;
        dest.sin_port = htons(NETWORK_PORT);
        inet_pton(AF_INET, current->ip, &dest.sin_addr);

        connect(sock_fd, (struct sockaddr *)&dest, addr_len);

	sendto(sock_fd, message, sizeof(message), 0,
            (struct sockaddr *)&dest, addr_len);

        close(sock_fd);
    }
}

void tcp_request_network_connection() {
    int sock_fd = 0, addr_len = 0;
    struct sockaddr_in dest;
    char message[48];

    int i = 0, j = 0;
    for (; i < strlen(LIST_REQUEST); i++) {
        message[i] = LIST_REQUEST[i];
    }
    for (; j < strlen(network_name); i++, j++) {
        message[i] = network_name[j];
    }
    message[i++] = '\n';
    message[i++] = '.';
    message[i] = '\n';

    addr_len = sizeof(struct sockaddr);

    dest.sin_family = AF_INET;
    dest.sin_port = htons(NETWORK_PORT);
    inet_pton(AF_INET, NETWORK_IP_ADDRESS, &dest.sin_addr);

    sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    connect(sock_fd, (struct sockaddr *)&dest, addr_len);

    printf("Requesting nodes list from %s:%u\n", 
        inet_ntoa(dest.sin_addr), ntohs(dest.sin_port));

	send(sock_fd, message, sizeof(message), 0);

    char received_data[1024];
	int bytes = recv(sock_fd, received_data, MAX_LIST_SIZE, 0);

    printf("Received %d bytes from %s:%u\n",
        bytes, inet_ntoa(dest.sin_addr), ntohs(dest.sin_port));

    int start, end = 0, nodes_num = 1;
    char name[32];

    while(received_data[end] != '\n') {
        name[end] = received_data[end];
        end++;
    }
    name[end] = '\0';

    printf("Received name = %s\n", name);

    struct node *new_node = (struct node *)malloc(sizeof(struct node));
	for (i = 0; name[i] != '\0'; i++) {
		new_node->name[i] = name[i];
	}
    sprintf(new_node->ip, "%s", inet_ntoa(dest.sin_addr));
	unsigned int port = ntohs(dest.sin_port);
	sprintf(new_node->port, "%u", port);

    array_list_add(nodes, new_node);

    printf("Added new node to list - %s:%s:%s\n",
        new_node->name, new_node->ip, new_node->port);

    end++;
    start = end;

    if (strncmp(received_data + end, "\n.\n", 3) == 0) {
        for (; end < MAX_LIST_SIZE; end++) {
            if (received_data[end] == '\n') {
                struct node* new_node = (struct node *)malloc(sizeof(struct node));

                i = 0;
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
                nodes_num++;
                start = end + 1;
            }
            if (strncmp(received_data + end, "\n.\n", 3) == 0) {
                break;
            }
        }
    }

    printf("List of %d nodes successfully received from %s!\n",
        nodes_num, name);

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
    server_addr.sin_port = htons(NETWORK_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(master_sock_fd, (struct sockaddr *) &server_addr, addr_len);

    listen(master_sock_fd, 5);

    while (1) {
        printf("Listening TCP connections on port %d\n", NETWORK_PORT);
        
        memset(data_buffer, 0, sizeof(data_buffer));
        FD_ZERO(&readfds);
        FD_SET(master_sock_fd, &readfds);

        select(master_sock_fd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(master_sock_fd, &readfds)) {
            comm_sock_fd = accept(master_sock_fd, (struct sockaddr *) &client_addr, &addr_len);

            recvfrom(comm_sock_fd, (char *) data_buffer, sizeof(data_buffer), 0,
                (struct sockaddr *) &client_addr, &addr_len);

            if (strncmp(data_buffer, LIST_REQUEST, strlen(LIST_REQUEST)) == 0) {
                char name[32];
                int i = strlen(LIST_REQUEST) + 1, j = 0, k;
                while (data_buffer[i] != '\n') {
                    name[j++] = data_buffer[i++];
                }

                printf("[NEW NODE] %s:%s:%u requests list of nodes\n", 
                    name, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                // Send list
                struct node *current;
                i = array_list_iter(nodes), j = 0;
                memset(data_buffer, 0, sizeof(data_buffer));
                while (network_name[j] != '\0') {
                    data_buffer[j] = network_name[j];
                    j++;
                }
                data_buffer[j++] = '\n';
                for (; i != -1; i = array_list_next(nodes, i)) {
                    current = array_list_get(nodes, i);

                    for (k = 0; current->name[k] != '\0'; j++, k++) {
		                data_buffer[j] = current->name[k];
	                }
                    data_buffer[j++] = ':';
                    for (k = 0; current->ip[k] != '\0'; j++, k++) {
		                data_buffer[j] = current->ip[k];
	                }
                    data_buffer[j++] = ':';
                    for (k = 0; current->port[k] != '\0'; j++, k++) {
		                data_buffer[j] = current->port[k];
	                }
                    data_buffer[j++] = '\n';
                }

                data_buffer[j++] = '.';
                data_buffer[j] = '\n';

                int bytes = sendto(comm_sock_fd, data_buffer, sizeof(data_buffer), 0,
                    (struct sockaddr *) &client_addr, addr_len);

                struct node *new_node = (struct node *)malloc(sizeof(struct node));
		        for (; name[i] != '\0'; i++) {
			        new_node->name[i] = name[i];
		        }
		        sprintf(new_node->ip, "%s", inet_ntoa(client_addr.sin_addr));
		        unsigned int port = ntohs(client_addr.sin_port);
		        sprintf(new_node->port, "%u", port);

                new_node_info(new_node);
                array_list_add(nodes, new_node);

                printf("Successfully sent list (%d bytes) to %s and added him to list of nodes\n",
                    bytes, name);
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

                printf("[NEW NODE] %s:%u informs about new node - %s:%s:%s\n", 
                    inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
                        new_node->name, new_node->ip, new_node->port);

                array_list_add(nodes, new_node);
            }
        }
    }
}

int main(int argc, char **argv) {
    printf("Your Network name: ");
    scanf("%s", network_name);

    char answer = '0';
    while (answer != 'y' && answer != 'n') {
        printf("Are you first node in network? [y/n] ");
        scanf(" %c",&answer);
        getchar();
    }

    nodes = create_array_list(16);
    pthread_t udp_send_ping_thread;
    pthread_t udp_receive_ping_thread;

    if (answer == 'n') {
        // Ask for list of connected nodes
        tcp_request_network_connection();
    }

    // Create PING/PONG loops
    pthread_create(&udp_send_ping_thread, NULL, udp_send_ping, NULL);
    pthread_create(&udp_receive_ping_thread, NULL, udp_receive_ping, NULL);

    // Listen for new nodes info
    tcp_listen();
    return 0;
}
