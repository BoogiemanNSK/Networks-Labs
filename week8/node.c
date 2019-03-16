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
#define MAX_FILE_SIZE 4096
#define MAX_PATH_SIZE 64
#define STARTING_ARRAY_LIST_SIZE 16
#define PING_LIMIT 10

// RELATIVE PATHS
#define SHARED_FOLDER_PATH "Shared"
#define REMOVED_FOLDER_PATH "Removed"

// GLOBALS
p_array_list nodes;
p_array_list files;
char network_name[NAME_SIZE];

// Struct that stores nodes info
struct node {
    char name[NAME_SIZE];
    char ip[IP_PORT_SIZE];
    char port[IP_PORT_SIZE];
    int ping;
};

// Struct that stores files info
struct file {
    char path[MAX_PATH_SIZE];
    int size;
};

////// FILES INTERACTION //////

// Check if file with given name (path) is in local library
// 0 - No, 1 - Yes
int check_local_file(char *path) {
    DIR *d;
    struct dirent *dir;
    d = opendir(SHARED_FOLDER_PATH);
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

// Check if file with given name (path) is in shared library
// 0 - No, 1 - Yes
int check_shared_file(char *path) {
    struct file *current;
    for (int i = array_list_iter(files); i != -1; i = array_list_next(files, i)) {
        current = array_list_get(files, i);
        if (strcmp(current->path, path) == 0) {
            return 1;
        }
    }
    return 0;
}

void print_file_library() {
    printf("\n=== SHARED FILE LIBRARY ===\n");
    
    int k = 0;
    struct file *current;
    for (int i = array_list_iter(files); i != -1; i = array_list_next(files, i), k++) {
        current = array_list_get(files, i);
        printf("[%d] - %s (%d words)\n", k, current->path, current->size);
    }

    printf("===========================\n");
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

void load_file_library() {
    DIR *d;
    struct dirent *dir;
    d = opendir(SHARED_FOLDER_PATH);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if (dir->d_name[0] == '.') { continue; }

            struct file *new_file = (struct file *)malloc(sizeof(struct file));

            strcpy(new_file->path, dir->d_name);

            char *temp_name = malloc(MAX_PATH_SIZE);
            temp_name[0] = '\0';
            strcat(temp_name, SHARED_FOLDER_PATH);
            strcat(temp_name, "/");        
            strcat(temp_name, new_file->path);
            new_file->size = count_words(temp_name);
            free(temp_name);

            array_list_add(files, new_file);
        }
        closedir(d);
    }
}

////// UDP CONNECTIONS //////

// Reply to every PING by PONG and for every PONG
// set allowed num of missed PINGs to PING_LIMIT
// [Uses separate thread #1]
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

// Send PING to every node in list
// [Uses separate thread #2]
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

// Compares state of shared files folder with network file list
// If found any changes - report them to all known nodes
// [Uses separate thread #3]
void * scan_file_library(void *arg) {
    // TODO
}

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

    printf("Connecting to %s:%u...\n", 
        inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));

    connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

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
        strncpy(new_node->port, message + start, end - start);
        end++; start = end;

        array_list_add(nodes, new_node);
    }

    printf("[OK] List of %d nodes successfully received from %s!\n",
        nodes_num, server_name);

    // Build request and your own list (from array list)
    message[0] = '\0';
    strcat(message, FILE_LIST_REQUEST);
    struct file *current;
    for (int i = array_list_iter(files); i != -1; i = array_list_next(files, i)) {
        current = array_list_get(files, i);

        strcat(message, current->path);
        strcat(message, ":");
        
        char size_str[4];
        sprintf(size_str, "%d", current->size);
        
        strcat(message, size_str);
        strcat(message, "\n");
    }
    strcat(message, END_OF_MSG);

    write(sock_fd, message, MAX_BUFFER_SIZE);
	read(sock_fd, message, MAX_BUFFER_SIZE);

    // Add other host's files to your array list
    start = 0, end = 0;
    while (strncmp(message + end, END_OF_MSG, strlen(END_OF_MSG) != 0)) {
        current = (struct file *)malloc(sizeof(struct file));

        while (message[end] != ':') { end++; }
        strncpy(current->path, message + start, end - start);
        end++; start = end;

        char size_str[4];
        while (message[end] != '\n') { end++; }
        strncpy(size_str, message + start, end - start);
        current->size = atoi(size_str);
        end++; start = end;

        array_list_add(files, current);
    }

    printf("[OK] Successfully synced file libraries\n");

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

    free(message);
}

// Synchronize lists with host from given socket
void sync_list_of_files(int sock_fd) {
    char *message = malloc(MAX_BUFFER_SIZE);
    char *local_list = malloc(MAX_BUFFER_SIZE);
    struct file *current;
    
    // Read list from other host
    read(sock_fd, message, MAX_BUFFER_SIZE);
    if (strncmp(message, FILE_LIST_REQUEST, strlen(FILE_LIST_REQUEST)) != 0) {
        printf("[ERR] In command of files receive request\n");
        return;
    }

    // Build your own list (from array list)
    local_list[0] = '\0';
    for (int i = array_list_iter(files); i != -1; i = array_list_next(files, i)) {
        current = array_list_get(files, i);

        strcat(local_list, current->path);
        strcat(local_list, ":");
        char size_str[4];
        sprintf(size_str, "%d", current->size);
        strcat(local_list, size_str);
        strcat(local_list, "\n");
    }
    strcat(local_list, END_OF_MSG);

    // Add other host's files to your array list
    int files_num = 1, start = strlen(FILE_LIST_REQUEST), end = start;
    while (strncmp(message + end, END_OF_MSG, strlen(END_OF_MSG)) != 0) {
        current = (struct file *)malloc(sizeof(struct file));
        files_num++;    

        while (message[end] != ':') { end++; }
        strncpy(current->path, message + start, end - start);
        end++; start = end;

        char size_str[4];
        while (message[end] != '\n') { end++; }
        strncpy(size_str, message + start, end - start);
        current->size = atoi(size_str);
        end++; start = end;

        array_list_add(files, current);
    }

    // Send previously built local list to host, so he could sync
    write(sock_fd, local_list, MAX_BUFFER_SIZE);

    printf("[OK] Successfully synced file libraries\n");

    free(message);
    free(local_list);
}

// Ask user for file path and then ask every known node for it
void file_download() {
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
        char answer = '0';
        while (answer != 'n') {
            print_file_library();
            printf("Do you want to download any files? [y/n] ");
            scanf(" %c", &answer);
            if (answer == 'y') {
                file_download();
            }
            getchar();
        }

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
    files = create_array_list(STARTING_ARRAY_LIST_SIZE);
    load_file_library();

    pthread_t udp_send_ping_thread;
    pthread_t udp_receive_ping_thread;
    // pthread_t file_library_monitoring_thread;

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

    // Create thread to monitor changes in file library
    // pthread_create(&file_library_monitoring_thread, NULL, scan_file_library, NULL);

    // Listen for new nodes info
    tcp_listen();
    return 0;
}
