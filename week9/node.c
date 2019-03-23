#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <dirent.h>

#include "alist.h"

// PORT
#define MY_PORT "12000"

// COMMANDS
#define SYNC_MSG "1"
#define REQUEST_MSG "0"

// FIXED SIZES
#define NAME_SIZE 32
#define IP_PORT_SIZE 16
#define MAX_BUFFER_SIZE 1024
#define MAX_FILE_SIZE 4096
#define MAX_PATH_SIZE 64
#define STARTING_ARRAY_LIST_SIZE 16
#define PING_LIMIT 10

// STRINGS
#define FILES_LIBRARY_PATH "Shared"
#define NETWORK_IF "eth1"

// GLOBALS
p_array_list nodes;
char network_name[NAME_SIZE];

// Struct that stores nodes info
struct node {
    char name[NAME_SIZE];
    char ip[IP_PORT_SIZE];
    char port[IP_PORT_SIZE];

    char **files;
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

// Counts number of spaces or '\n' + 1 in a given file
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

// Returns IP string
char *get_my_ip() {
    char *ip_address = malloc(IP_PORT_SIZE);
    int fd;
    struct ifreq ifr;
    
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    ifr.ifr_addr.sa_family = AF_INET;
    memcpy(ifr.ifr_name, NETWORK_IF, IFNAMSIZ - 1);
    ioctl(fd, SIOCGIFADDR, &ifr);
    strcpy((char *) ip_address, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

    close(fd);
    return ip_address;
}

// Returns names of files in local library
char **get_local_files() {
    DIR *d;
    struct dirent *dir;
    char **files = malloc(MAX_BUFFER_SIZE / MAX_PATH_SIZE);
    int i = 0;

    d = opendir(FILES_LIBRARY_PATH);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            files[i] = malloc(MAX_PATH_SIZE);
            files[i][0] = '\0';
            strcat(files[i], dir->d_name);
            i++;
        }
        closedir(d);
    }
    
    files[i] = NULL;
    return files;
}

// Returns pointer to node from its string, creates new node if not exist
struct node *get_node_by_string(char *str) {
    char *temp = malloc(NAME_SIZE);
    struct node *current;
    
    int t = 0;
    while (str[t] != ':') {
        temp[t] = str[t];
        t++;
    }
    temp[t] = '\0';

    int exist = 0;
    for (int i = array_list_iter(nodes); i != -1; i = array_list_next(nodes, i)) {
        current = array_list_get(nodes, i);
        if (strncmp(temp, current->name, strlen(temp)) == 0) {
            exist = 1;
            break;
        }
    }

    if (exist == 0) {
        struct node *new_node = malloc(sizeof(struct node));
        strncpy(new_node->name, temp, strlen(temp));
        
        int i = 0;
        t++;
        while (str[t] != ':') {
            temp[i] = str[t];
            i++; t++;
        }
        temp[i] = '\0';
        strncpy(new_node->ip, temp, strlen(temp));

        i = 0;
        t++;
        while (str[t] != ':') {
            temp[i] = str[t];
            i++; t++;
        }
        temp[i] = '\0';
        strncpy(new_node->port, temp, strlen(temp));

        new_node->files = malloc(MAX_BUFFER_SIZE / MAX_PATH_SIZE);
        new_node->files[0] = NULL;

        array_list_add(nodes, new_node);
        current = new_node;
    }

    free(temp);
    return current;
}

void rewrite_files(struct node *p, char *msg) {
    int i = 0, k = 0, t;
    for (int j = 0; j < 3; j++) {
        while (msg[i] != ':') { i++; }
        i++;
    }

    while(msg[i] != '\0') {
        t = 0;
        while (msg[i] != ',' && msg[i] != '\0') {
            p->files[k][t] = msg[i];
            t++; i++;
        }
        p->files[k][t] = '\0';
        k++;
    }
    p->files[k] = NULL;
}


////// TCP CONNECTIONS //////

// Send list of known nodes to all nodes
void * send_sync(void *arg) {
    char *message = malloc(MAX_BUFFER_SIZE);
    struct node *current, *cur;
    struct sockaddr_in server_addr;

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    while(1){
        sleep(2);

        for (int i = array_list_iter(nodes); i != -1; i = array_list_next(nodes, i)) {
            current = array_list_get(nodes, i);
            int port = atoi(current->port);

            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(port);
            inet_pton(AF_INET, current->ip, &server_addr.sin_addr);

            connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

            write(sock_fd, SYNC_MSG, strlen(SYNC_MSG));

            char *my_ip = get_my_ip();
            char **files_lib = get_local_files();

            message[0] = '\0';
            strcat(message, network_name);
            strcat(message, ":");
            strcat(message, my_ip);
            strcat(message, ":");
            strcat(message, MY_PORT);

            if (files_lib[0] == NULL) {
                strcat(message, ":");
            }

            int k = 0;
            while (files_lib[k] != NULL) {
                strcat(message, ":");
                strcat(message, files_lib[k]);
                k++;
            }

            write(sock_fd, message, MAX_BUFFER_SIZE);
            free(my_ip);
            free(files_lib);

            char size_str[4];
            sprintf(size_str, "%d", (int)nodes->count);
            write(sock_fd, size_str, 4);

            for (int p = array_list_iter(nodes); p != -1; p = array_list_next(nodes, p)) {
                cur = array_list_get(nodes, p);

                message[0] = '\0';
                strcat(message, current->name);
                strcat(message, ":");
                strcat(message, current->ip);
                strcat(message, ":");
                strcat(message, current->port);
                strcat(message, ":");

                write(sock_fd, message, MAX_BUFFER_SIZE);
            }
        }
    }

    free(message);
}

// On receiving sync, compare your list and received
void receive_sync(int comm_fd) {
    char *message = malloc(MAX_BUFFER_SIZE);
    struct node *current;
    int n;

    read(comm_fd, message, MAX_BUFFER_SIZE);
    current = get_node_by_string(message);
    rewrite_files(current, message);

    read(comm_fd, message, MAX_BUFFER_SIZE);
    n = atoi(message);

    for (int i = 0; i < n; i++) {
        read(comm_fd, message, MAX_BUFFER_SIZE);
        get_node_by_string(message);
    }

    free(message);
}

// Ask all known nodes for file and download it
void * send_request(void *arg) {
    char answer = '0';
    while (answer != 'y' && answer != 'n') {
        printf("Do you want to download any files? [y/n] ");
        scanf(" %c", &answer);
        getchar();
    }

    if (answer == 'n') {
        return NULL;
    }

    char *filename = malloc(MAX_PATH_SIZE);
    printf("Specify filename: ");
    scanf("%s", filename);

    int sock_fd = 0, i = array_list_iter(nodes);
    struct sockaddr_in server_addr;
    struct node *current;
    char *message = malloc(MAX_FILE_SIZE);

    if (check_local_file(filename) == 1) {
        printf("[ERR] You already have that file\n");
        return NULL;
    }

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    int found = 0;
    for (; i != -1; i = array_list_next(nodes, i)) {
        current = array_list_get(nodes, i);

        int j = 0;
        while (current->files[j] != NULL) {
            if (strncmp(filename, current->files[j], strlen(filename)) == 0) {
                found = 1;
                break;
            }  
            j++;
        }

        if (found) {
            break;
        }
    }

    if (!found) {
        printf("[ERR] No such file exist across known nodes libraries\n");
        return NULL;
    } 

    int port = atoi(current->port);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, current->ip, &server_addr.sin_addr);

    connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

	write(sock_fd, REQUEST_MSG, strlen(REQUEST_MSG));
    write(sock_fd, filename, strlen(filename));

    read(sock_fd, message, MAX_BUFFER_SIZE);
    int size = atoi(message);
    
    char *path = malloc(MAX_PATH_SIZE);
    path[0] = '\0';
    strcat(path, FILES_LIBRARY_PATH);
    strcat(path, "/");
    strcat(path, filename);

    FILE *fp = fopen(path, "ab+");
    for (int i = 0; i < size; i++) {
        read(sock_fd, message, MAX_BUFFER_SIZE);
        fprintf(fp, "%s ", message);
    }
    fclose(fp);

    free(filename);
    free(path);
    free(message);
}

// Reply to request of file (it exists for sure)
void upload_file(int sock_fd, char *file_name) {
    char *temp_name = malloc(MAX_PATH_SIZE);
    temp_name[0] = '\0';
    strcat(temp_name, FILES_LIBRARY_PATH);
    strcat(temp_name, "/");        
    strcat(temp_name, file_name);
    int size = count_words(temp_name);

    char size_str[4];
    sprintf(size_str, "%d", size);
    write(sock_fd, size_str, 4);

    FILE *f;
    f = fopen(temp_name, "r");

    char *temp_buffer = malloc(MAX_BUFFER_SIZE);
    for (int i = 0; i < size; i++) {
        memset(temp_buffer, 0, MAX_BUFFER_SIZE);
        fscanf(f, "%s", temp_buffer);
        write(sock_fd, temp_buffer, MAX_BUFFER_SIZE);
    }
    
    fclose(f);
    free(temp_name);
    free(temp_buffer);
}

// Reply to incoming connection
void * reply(void *arg) {
    char *message = malloc(MAX_BUFFER_SIZE);
    int comm_fd = *(int*)(arg);

    read(comm_fd, message, MAX_BUFFER_SIZE);

    if (strncmp(message, REQUEST_MSG, strlen(REQUEST_MSG)) == 0) {
        char *filename = malloc(MAX_PATH_SIZE);

        read(comm_fd, message, MAX_BUFFER_SIZE);
        int filename_size = 0;
        while (message[filename_size] != '\0') {
            filename[filename_size] = message[filename_size];
            filename_size++;
        }
        filename[filename_size] = '\0';

        upload_file(comm_fd, filename);
    } else if (strncmp(message, SYNC_MSG, strlen(SYNC_MSG)) == 0) {
        receive_sync(comm_fd);
    } else {
        printf("[ERROR] Could not recognize received message: %s\n", message);
    }

    free(message);
}

// Wait for incoming TCP connection and reply, depending on command received
void wait_requests() {
    int master_sock_fd = 0, comm_sock_fd = 0, addr_len = 0, p = 0, port;
    struct sockaddr_in server_addr, client_addr;
    pthread_t threads[20];

    port = atoi(MY_PORT);
    addr_len = sizeof(client_addr);
    master_sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(master_sock_fd, (struct sockaddr *) &server_addr, sizeof(server_addr));

    listen(master_sock_fd, 20);

    while (1) {
        memset(&client_addr, 0, sizeof(client_addr));
        comm_sock_fd = accept(master_sock_fd, (struct sockaddr *) &client_addr, &addr_len);
        pthread_create(&threads[p % 20], NULL, reply, &comm_sock_fd);
    }
}

void first_sync(char *ip_port) {
    char *message = malloc(MAX_BUFFER_SIZE);
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    char *ip = malloc(IP_PORT_SIZE);
    char *port = malloc(IP_PORT_SIZE);

    int i = 0, j = 0;
    while(ip_port[i] != ':') {
        ip[j] = ip_port[i];
        i++; j++;
    }
    i++; j = 0;
    while(ip_port[i] != '\0') {
        port[j] = ip_port[i];
        i++; j++;
    } 

    int port_i = atoi(port);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_i);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    write(sock_fd, SYNC_MSG, strlen(SYNC_MSG));

    char *my_ip = get_my_ip();
    char **files_lib = get_local_files();

    message[0] = '\0';
    strcat(message, network_name);
    strcat(message, ":");
    strcat(message, my_ip);
    strcat(message, ":");
    strcat(message, MY_PORT);

    if (files_lib[0] == NULL) {
        strcat(message, ":");
    }

    int k = 0;
    while (files_lib[k] != NULL) {
        strcat(message, ":");
        strcat(message, files_lib[k]);
        k++;
    }

    write(sock_fd, message, MAX_BUFFER_SIZE);
    write(sock_fd, "0", 4);

    free(ip);
    free(port);
    free(my_ip);
    free(files_lib);
}


////// MAIN //////

int main() {
    printf("|------| Faraday P2P Node v3.0 |------|\n\n");

    printf("Your Network name: ");
    scanf("%s", network_name);

    char answer = '0';
    while (answer != 'y' && answer != 'n') {
        printf("Are you first node in network? [y/n] ");
        scanf(" %c", &answer);
        getchar();
    }

    nodes = create_array_list(STARTING_ARRAY_LIST_SIZE);

    pthread_t send_sync_thread, file_download_thread;

    // Ask specidied ip for list of connected nodes
    if (answer == 'n') {
        char input[IP_PORT_SIZE * 2];
        printf("Please, specify IP:PORT of node inside network: ");
        scanf("%s", input);
        first_sync(input);
    }

    // Create PING/PONG loops
    pthread_create(&send_sync_thread, NULL, send_sync, NULL);
    pthread_create(&file_download_thread, NULL, send_request, NULL);

    // Listen for new connections
    printf("Listening for new connections on %s...\n", MY_PORT);
    wait_requests();
    return 0;
}
