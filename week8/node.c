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

// PORTS
#define NETWORK_PORT 12000
#define PING_PORT 12100

// COMMANDS
#define HELLO_MSG "HELO\n"
#define NODE_LIST_REQUEST "GET_NODE_LIST\n"
#define FILE_LIST_REQUEST "SYNC_FILE_LIST\n"
#define FILE_RETRIEVE_REQUEST "RETR\n"
#define FILE_ADD_MSG "ADD_FILE\n"
#define FILE_REMOVE_MSG "REM_FILE\n"
#define NEW_NODE_MSG "NEW_NODE\n"
#define OK_MSG "OK\n"
#define ERR_MSG "ERR\n"
#define END_OF_MSG ".\n"
#define PING "ping"
#define PONG "pong"

// FIXED SIZES
#define NAME_SIZE 32
#define IP_PORT_SIZE 16
#define MAX_BUFFER_SIZE 2048
#define STARTING_NODES_ARRAY_SIZE 16
#define PING_LIMIT 5

// GLOBALS
p_array_list nodes;
char network_name[NAME_SIZE];

// Struct that stores nodes info
struct node {
    char name[NAME_SIZE];
    char ip[IP_PORT_SIZE];
    char port[IP_PORT_SIZE];
    int ping;
};

////// UDP CONNECTIONS //////

// Reply to every PING by PONG [uses separate thread #1]
void * udp_receive_ping(void *arg) {
    char ping_buffer[strlen(PING) + 1];
    int sock_fd = 0, addr_len = 0;
    struct sockaddr_in server_addr, client_addr;
    struct node *current;

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PING_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock_fd, (struct sockaddr *) &server_addr, sizeof(server_addr));

    while (1) {
        memset(&client_addr, 0, sizeof(client_addr));

        recvfrom(sock_fd, ping_buffer, strlen(PING) + 1, 0,
            (struct sockaddr *) &client_addr, &addr_len);

        if (strncmp(PING, ping_buffer, strlen(PING)) == 0) {
            sendto(sock_fd, PONG, strlen(PING) + 1, 0,
		        (struct sockaddr *) &client_addr, addr_len);
        } else if (strncmp(PONG, ping_buffer, strlen(PONG)) == 0) {
            char *client_ip = inet_ntoa(client_addr.sin_addr);            
            for (int i = array_list_iter(nodes); i != -1; i = array_list_next(nodes, i)) {
                current = array_list_get(nodes, i);

                if (strncmp(current->ip, client_ip, strlen(client_ip)) == 0) {
                    current->ping = 5;
                    break;
                }
            }
        }
    }
}

// Send PING to every node in list [uses separate thread #2]
void * udp_send_ping(void *arg) {
    int sock_fd = 0;
    struct sockaddr_in server_addr;
    struct node *current;

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);

    while(1) {
        sleep(2);

        for (int i = array_list_iter(nodes); i != -1; i = array_list_next(nodes, i)) {
            current = array_list_get(nodes, i);

            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(PING_PORT);
            inet_pton(AF_INET, current->ip, &server_addr.sin_addr);

	        printf("[PING] %s:%s:%u SENT {%d}\n", current->name, current->ip, PING_PORT, current->ping);

	        sendto(sock_fd, PING, strlen(PING) + 1, 0,
		        (struct sockaddr *)&server_addr, sizeof(server_addr));

            current->ping--;

            if (current->ping < 0) {
                printf("[PING] %s:%s:%u did not respond to %d PINGs - Dropping from list\n", current->name, current->ip, PING_PORT, PING_LIMIT);
                array_list_remove(nodes, current);
                free(current);
            }
        }
    }
}

/////// TCP CONNECTIONS //////

// Send to every known node info about new connected (to me) node
// And then add it to my list of known nodes
void new_node_info(struct node *new_node) {
    char *message = malloc(MAX_BUFFER_SIZE);
    int sock_fd = 0, i = array_list_iter(nodes), j = 0;;
    struct sockaddr_in server_addr;
    struct node *current;

    message[0] = '\0';
    strcat(message, NEW_NODE_MSG);
    strcat(message, new_node->name);
    strcat(message, ":");
    strcat(message, new_node->ip);
    strcat(message, ":");
    strcat(message, new_node->port);
    strcat(message, "\n");
    strcat(message, END_OF_MSG);

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    for (; i != -1; i = array_list_next(nodes, i)) {
        current = array_list_get(nodes, i);

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(NETWORK_PORT);
        inet_pton(AF_INET, current->ip, &server_addr.sin_addr);

        connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

	    write(sock_fd, message, MAX_BUFFER_SIZE);
        read(sock_fd, message, MAX_BUFFER_SIZE);

        if (strncmp(OK_MSG, message, strlen(OK_MSG)) == 0) {
            printf("[OK] New node notification successfully sent to %s!\n", current->name);
        } else if (strncmp(ERR_MSG, message, strlen(ERR_MSG)) == 0) {
            printf("[ERROR] That node is already known by %s and has other name\n", current->name);
        } else {
            printf("[ACK] %s did not respond correctly\n", current->name);
        }

        close(sock_fd);
    }

    free(message);
}

// Ask node with given IP for list of nodes in the network
void tcp_request_network_connection(char *ip_string) {
    char *message = malloc(MAX_BUFFER_SIZE);
    int sock_fd = 0;
    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NETWORK_PORT);
    inet_pton(AF_INET, ip_string, &server_addr.sin_addr);

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    printf("Connecting to %s:%u...\n", 
        inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));

    message[0] = '\0';
    strcat(message, HELLO_MSG);
    strcat(message, network_name);
    strcat(message, "\n");
    strcat(message, END_OF_MSG);

	write(sock_fd, message, MAX_BUFFER_SIZE);
	read(sock_fd, message, MAX_BUFFER_SIZE);

    int i = 0;
    char server_name[32];
    while(message[i] != '\n') {
        server_name[i] = message[i];
        i++;
    }
    server_name[i] = '\0';

    printf("[OK] %s is ready to send list of nodes -> Sending request\n", server_name);

    message[0] = '\0';
    strcat(message, NODE_LIST_REQUEST);
    strcat(message, END_OF_MSG);

	write(sock_fd, message, MAX_BUFFER_SIZE);
	read(sock_fd, message, MAX_BUFFER_SIZE);

    struct node *new_node = (struct node *)malloc(sizeof(struct node));
	strcpy(new_node->name, server_name);
    sprintf(new_node->ip, "%s", inet_ntoa(server_addr.sin_addr));
	unsigned int port = ntohs(server_addr.sin_port);
	sprintf(new_node->port, "%u", port);
    new_node->ping = PING_LIMIT;

    array_list_add(nodes, new_node);

    int nodes_num = 1, start = 0, end = 0;
    while (strncmp(message + end, END_OF_MSG, strlen(END_OF_MSG) != 0)) {
        new_node = (struct node *)malloc(sizeof(struct node));
        nodes_num++;

        while (message[end] != ':') { end++; }
        strncpy(new_node->name, message + start, end - start);
        end++; start = end;

        while (message[end] != ':') { end++; }
        strncpy(new_node->ip, message + start, end - start);
        end++; start = end;

        while (message[end] != '\n') { end++; }
        strncpy(new_node->name, message + start, end - start);
        end++; start = end;
    }

    printf("[OK] List of %d nodes successfully received from %s!\n",
        nodes_num, server_name);

    // TODO - FILE LIST SYNC

    close(sock_fd);
    free(message);
}

// Send list of known nodes to specified socket
void send_list_of_nodes(int sock_fd) {
    char *message = malloc(MAX_BUFFER_SIZE);
    struct node *current;
    
    message[0] = '\0';
    strcat(message, network_name);
    strcat(message, "\n");
    strcat(message, END_OF_MSG);

    write(sock_fd, message, MAX_BUFFER_SIZE);
    read(sock_fd, message, MAX_BUFFER_SIZE);

    if (strncmp(message, NODE_LIST_REQUEST, strlen(NODE_LIST_REQUEST)) == 0) {
        printf("[OK] Node list request received -> Sending list\n");
    }

    message[0] = '\0';
    for (int i = array_list_iter(nodes); i != -1; i = array_list_next(nodes, i)) {
        current = array_list_get(nodes, i);

        strcat(message, current->name);
        strcat(message, ":");
        strcat(message, current->ip);
        strcat(message, ":");
        strcat(message, current->port);
        strcat(message, "\n");
    }
    strcat(message, END_OF_MSG);

    write(sock_fd, message, MAX_BUFFER_SIZE);

    // TODO - FILE LIST SYNC

    free(message);
}

// Wait for incoming TCP connection and reply, depending on command received
void tcp_listen() {
    char *message = malloc(MAX_BUFFER_SIZE);
    int master_sock_fd = 0, comm_sock_fd = 0, addr_len = 0;
    struct sockaddr_in server_addr, client_addr;

    master_sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NETWORK_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(master_sock_fd, (struct sockaddr *) &server_addr, sizeof(server_addr));

    listen(master_sock_fd, 5);

    while (1) {
        printf("[TCP Port = %d | UDP Port = %d]\n", NETWORK_PORT, PING_PORT);
        
        message[0] = '\0';

        addr_len = sizeof(client_addr);
        comm_sock_fd = accept(master_sock_fd, (struct sockaddr *) &client_addr, &addr_len);

        read(comm_sock_fd, message, MAX_BUFFER_SIZE);

        if (strncmp(message, HELLO_MSG, strlen(HELLO_MSG)) == 0) {
            // Send list of nodes

            struct node *new_node = (struct node *)malloc(sizeof(struct node));
            int start = strlen(HELLO_MSG), end = start;

            while (message[end] != '\n') { end++; }
            strncpy(new_node->name, message + start, end - start);

            printf("[NEW NODE] %s:%s:%u requests list of nodes\n", 
                new_node->name, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            send_list_of_nodes(comm_sock_fd);
            
            new_node_info(new_node);

	        sprintf(new_node->ip, "%s", inet_ntoa(client_addr.sin_addr));
	        unsigned int port = ntohs(client_addr.sin_port);
	        sprintf(new_node->port, "%u", port);
            new_node->ping = PING_LIMIT;

            array_list_add(nodes, new_node);

            printf("[OK] Successfully sent list of nodes to %s and added him to list of nodes\n", new_node->name);
        } else if (strncmp(message, NEW_NODE_MSG, strlen(NEW_NODE_MSG)) == 0) {
            // Add new node to list
            
            struct node *new_node = (struct node *)malloc(sizeof(struct node));
            int start = strlen(NEW_NODE_MSG), end = start;
            
            while (message[end] != ':') { end++; }
            strncpy(new_node->name, message + start, end - start);
            end++; start = end;

            while (message[end] != ':') { end++; }
            strncpy(new_node->ip, message + start, end - start);
            end++; start = end;

            while (message[end] != '\n') { end++; }
            strncpy(new_node->name, message + start, end - start);
            end++; start = end;

            new_node->ping = PING_LIMIT;

            array_list_add(nodes, new_node);

            message[0] = '\0';
            strcat(message, OK_MSG);
            strcat(message, END_OF_MSG);
            write(comm_sock_fd, message, MAX_BUFFER_SIZE);

            printf("[NEW NODE] %s:%u informs about new node - %s:%s:%s\n", 
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
                    new_node->name, new_node->ip, new_node->port);
        } else if (strncmp(message, FILE_RETRIEVE_REQUEST, strlen(FILE_RETRIEVE_REQUEST)) == 0) {
            // TODO - FILE SENDER
        } else if (strncmp(message, FILE_ADD_MSG, strlen(FILE_ADD_MSG)) == 0) {
            // TODO - ADD FILE TO LIST
        } else if (strncmp(message, FILE_REMOVE_MSG, strlen(FILE_REMOVE_MSG)) == 0) {
            // TODO - REMOVE FILE FROM LIST (if in 'shared' folder - move to 'removed')
        } else {
            // Report error in received command

            strcat(message, ERR_MSG);
            strcat(message, END_OF_MSG);

            write(comm_sock_fd, message, MAX_BUFFER_SIZE);
        }
    }
}

////// MAIN //////

int main() {
    printf("Your Network name: ");
    scanf("%s", network_name);

    char answer = '0';
    while (answer != 'y' && answer != 'n') {
        printf("Are you first node in network? [y/n] ");
        scanf(" %c", &answer);
        getchar();
    }

    nodes = create_array_list(STARTING_NODES_ARRAY_SIZE);
    pthread_t udp_send_ping_thread;
    pthread_t udp_receive_ping_thread;

    // Ask specidied ip for list of connected nodes
    if (answer == 'n') {
        char input[IP_PORT_SIZE];
        printf("Please, specify IP of node inside network: ");
        scanf("%s", input);
        tcp_request_network_connection(input);
    }

    // Create PING/PONG loops
    pthread_create(&udp_send_ping_thread, NULL, udp_send_ping, NULL);
    pthread_create(&udp_receive_ping_thread, NULL, udp_receive_ping, NULL);

    // Listen for new nodes info
    tcp_listen();
    return 0;
}
