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
#include <dirent.h>

#include "alist.h"

// PORT
#define MY_PORT 12000

// COMMANDS
#define SYNC_MSG 1
#define REQUEST_MSG 0

// FIXED SIZES
#define NAME_SIZE 32
#define IP_PORT_SIZE 16
#define MAX_BUFFER_SIZE 1024
#define MAX_FILE_SIZE 4096
#define MAX_PATH_SIZE 64
#define STARTING_ARRAY_LIST_SIZE 16
#define PING_LIMIT 10

// PATHS
#define FILES_LIBRARY_PATH "Shared"

// GLOBALS
p_array_list nodes;
char network_name[NAME_SIZE];

// Struct that stores nodes info
struct node {
    char name[NAME_SIZE];
    char ip[IP_PORT_SIZE];
    char port[IP_PORT_SIZE];
};


////// HELPERS FUNCTIONS //////

// Check if file with given name (path) is in local library
// 0 - No, 1 - Yes
int check_local_file(char *path) {
    DIR *d;
    struct dirent *dir;
    d = opendir(FILES_LIBRARY_PATH);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if (strcmp(dir->d_name, path) == 0) {
                return 1;
            }
        }
        closedir(d);
    }
    return 0;
}

int count_words(char *path) {
    FILE * f;
    f = fopen(path, "r");
    int count = 1;
    char c = fgetc(f);
    while(c != EOF) {
        if (c == ' ' || c == '\n') { count++; }
        c = fgetc(f);
    }
    fclose(f);    
    return count;
}

////// TCP CONNECTIONS //////

// Send list of known nodes to all nodes
void send_sync() {
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

    free(message);
}

// On receiving sync, compare your list and received
void receive_sync() {

}

// Ask all known nodes for file and download it
void send_request(char *path) {
    char *path;
    char *message = malloc(MAX_BUFFER_SIZE);
    char *answer = malloc(MAX_FILE_SIZE);
    int sock_fd = 0, i = array_list_iter(nodes), j = 0;;
    struct sockaddr_in server_addr;
    struct node *current;
    printf("Please, specify name of file from list: ");
    scanf("%s", path);

    if (check_local_file(path) == 1) {
        printf("[ERR] You already have that file\n");
        return;
    }

    if (check_shared_file(path) == 0) {
        printf("[ERR] File with specified name does not exist in file library\n");
        return;
    }

    message[0] = '\0';
    strcat(message, FILE_RETRIEVE_REQUEST);
    strcat(message, path);
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
        read(sock_fd, answer, MAX_FILE_SIZE);

        if (strncmp(OK_MSG, answer, strlen(OK_MSG)) == 0) {
            // Node has requested file
            printf("[OK] %s has requested file -> Downloading\n", current->name);
            
            char *filename = malloc(MAX_PATH_SIZE);
            filename[0] = '\0';
            strcat(filename, SHARED_FOLDER_PATH);
            strcat(filename, "/");
            strcat(filename, path);
            FILE *fp = fopen(filename, "ab+");

            int start = strlen(OK_MSG), end = start, file_size = 0;
            
            while (answer[end] != '\n') { end++; }
            strncpy(message, answer + start, end - start);
            message[end - start] = '\0';
            file_size = atoi(message);

            for (int i = 0; i < file_size - 1; i++) {
                while (answer[end] != '\n') { end++; }
                strncpy(message, answer + start, end - start);
                message[end - start] = '\0';
                fprintf(fp, "%s ", message);
            }
            while (answer[end] != '\n') { end++; }
            strncpy(message, answer + start, end - start);
            message[end - start] = '\0';
            fprintf(fp, "%s", message);

            fclose(fp);
            free(filename);
            break;
        } else if (strncmp(ERR_MSG, answer, strlen(ERR_MSG)) == 0) {
            // Node does not have requested file
        } else {
            printf("[ACK] %s did not respond correctly\n", current->name);
        }

        close(sock_fd);
    }

    free(message);
    free(answer);
}

// Reply to request of file (it exists for sure)
void upload_file(int sock_fd, char *file_name) {
    char *answer = malloc(MAX_FILE_SIZE);
    answer[0] = '\0';
    strcat(answer, OK_MSG);

    char *temp_name = malloc(MAX_PATH_SIZE);
    temp_name[0] = '\0';
    strcat(temp_name, SHARED_FOLDER_PATH);
    strcat(temp_name, "/");        
    strcat(temp_name, file_name);
    int size = count_words(temp_name);

    char size_str[4];
    sprintf(size_str, "%d", size);
    strcat(answer, size_str);
    strcat(answer, "\n");

    FILE *f;
    f = fopen(temp_name, "r");

    char *temp_buffer = malloc(MAX_BUFFER_SIZE);
    for (int i = 0; i < size; i++) {
        memset(temp_buffer, 0, MAX_BUFFER_SIZE);
        fscanf(f, "%s", temp_buffer);
        strcat(answer, temp_buffer);
        strcat(answer, "\n");
    }

    strcat(answer, END_OF_MSG);
    write(sock_fd, answer, MAX_FILE_SIZE);
    
    fclose(f);
    free(temp_name);
    free(temp_buffer);
    free(answer);
}

// Wait for incoming TCP connection and reply, depending on command received
void wait_connections() {
    char *message = malloc(MAX_BUFFER_SIZE);
    int master_sock_fd = 0, comm_sock_fd = 0, addr_len = 0;
    struct sockaddr_in server_addr, client_addr;

    master_sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(MY_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(master_sock_fd, (struct sockaddr *) &server_addr, sizeof(server_addr));

    listen(master_sock_fd, 20);

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
            sync_list_of_files(comm_sock_fd);
            
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
            int start = strlen(FILE_RETRIEVE_REQUEST), end = start;
            char *file_name = malloc(MAX_PATH_SIZE);
            
            while (message[end] != '\n') { end++; }
            strncpy(file_name, message + start, end - start);

            if (check_local_file(file_name) == 1) {
                printf("[FILE REQUEST] %s:%u asks for file %s -> Sending it\n",
                    inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), file_name);
                upload_file(comm_sock_fd, file_name);
                printf("[OK] Successfully uploaded file\n");
            } else {
                message[0] = '\0';
                strcat(message, ERR_MSG);
                strcat(message, END_OF_MSG);
                write(comm_sock_fd, message, MAX_BUFFER_SIZE);
            }

            free (file_name);
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

    nodes = create_array_list(STARTING_ARRAY_LIST_SIZE);

    pthread_t send_sync_thread;
    pthread_t receive_sync_thread;

    // Ask specidied ip for list of connected nodes
    if (answer == 'n') {
        char input[IP_PORT_SIZE * 2];
        printf("Please, specify IP:PORT of node inside network: ");
        scanf("%s", input);
        first_sync(input);
    }

    // Create PING/PONG loops
    pthread_create(&send_sync_thread, NULL, send_sync, NULL);
    pthread_create(&receive_sync_thread, NULL, receive_sync, NULL);

    answer = '0';
    while (answer != 'y' && answer != 'n') {
        printf("Do you want to download any files? [y/n] ");
        scanf(" %c", &answer);
        getchar();
    }

    char *filename = malloc(MAX_PATH_SIZE);
    if (answer == 'y') {
        printf("Specify filename: ");
        scanf("%s", filename);
        send_request(filename);
    }

    // Listen for new nodes info
    wait_connections();
    return 0;
}
