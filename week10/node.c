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
#include <unistd.h>
#include <fcntl.h>

#include "helpers.h"


// GLOBALS
p_array_list nodes;
char network_name[NAME_SIZE];
char* network_ip;


// Send list of known nodes to all nodes
void * send_sync(void *arg) {
    int sock_fd;

    int msg_int = SYNC_MSG_INT;
    char *msg_str = malloc(MAX_BUFFER_SIZE);

    struct node *current, *cur;
    struct sockaddr_in server_addr;
    

    while(1) {
        sleep(2);

        for (int i = array_list_iter(nodes); i != -1; i = array_list_next(nodes, i)) {
            printf("[SEND SYNC] BEGIN\n");

            current = array_list_get(nodes, i);

            printf("[SEND SYNC] Sending sync to %s:%s:%s...\n", current->name, current->ip, current->port);

            sock_fd = socket(AF_INET, SOCK_STREAM, 0);

            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(atoi(current->port));
            inet_pton(AF_INET, current->ip, &server_addr.sin_addr);

            if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
                printf("[ERROR] Couldn't connect to %s\n", current->name);
                continue;
            }

            stringify_me(msg_str, network_name, network_ip);

            printf("[SEND SYNC] Stringify res: %s\n", msg_str);

            write_sync(sock_fd, &msg_int, sizeof(int));
            write_sync(sock_fd, msg_str, MAX_BUFFER_SIZE);
            write_sync(sock_fd, (int*)&nodes->count, sizeof(int));

            for (int p = array_list_iter(nodes); p != -1; p = array_list_next(nodes, p)) {
                cur = array_list_get(nodes, p);

                stringify_node(msg_str, cur);
                printf("[SEND SYNC] Stringify res: %s\n", msg_str);
                write_sync(sock_fd, msg_str, MAX_BUFFER_SIZE);
            }

            close(sock_fd);
            printf("[SEND SYNC] END\n");
        }
    }
}


// On receiving sync, compare your list and received
void receive_sync(int comm_fd) {
    int msg_int;
    char *msg_str = malloc(MAX_BUFFER_SIZE);
    
    struct node *current;

    printf("[RECV SYNC] BEGIN\n");
    read(comm_fd, msg_str, MAX_BUFFER_SIZE);

    printf("[RECV SYNC] Received sync from %s\n", msg_str);
    current = get_node_by_string(msg_str, network_name, nodes);
    rewrite_files(current, msg_str);

    printf("[RECV SYNC] Files rewritten\n");
    read(comm_fd, &msg_int, MAX_BUFFER_SIZE);

    printf("[RECV SYNC] Number of nodes to receive: %d\n", msg_int);
    for (int i = 0; i < msg_int; i++) {
        read(comm_fd, msg_str, MAX_BUFFER_SIZE);
        printf("[RECV SYNC] New node - %s\n", msg_str);
        current = get_node_by_string(msg_str, network_name, nodes);
    }

    printf("[RECV SYNC] END\n");

    free(msg_str);
}


// Ask all known nodes for file and download it
void * send_request(void *arg) {
    int sock_fd = 0, i = array_list_iter(nodes), msg_int = REQUEST_MSG_INT;
    struct sockaddr_in server_addr;
    struct node *current;

    if (get_answer("Do you want to download any files? [y/n]") == 'n') {
        return NULL;
    }

    char *path = malloc(MAX_BUFFER_SIZE);
    char *msg_str = malloc(MAX_BUFFER_SIZE);
    char *filename = malloc(MAX_BUFFER_SIZE);
    printf("Specify filename: ");
    scanf("%s", filename);
    
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

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(current->port));
    inet_pton(AF_INET, current->ip, &server_addr.sin_addr);

    connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

	write_sync(sock_fd, &msg_int, sizeof(int));
    write_sync(sock_fd, filename, MAX_BUFFER_SIZE);
    read(sock_fd, &msg_int, MAX_BUFFER_SIZE);

    sprintf(path, "%s/%s", FILES_LIBRARY_PATH, filename);
    
    FILE *fp = fopen(path, "ab+");
    for (int i = 0; i < msg_int; i++) {
        read(sock_fd, msg_str, MAX_BUFFER_SIZE);
        fprintf(fp, "%s ", msg_str);
    }
    fclose(fp);

    printf("Successfully downloaded %s\n", filename);

    close(sock_fd);

    free(filename);
    free(msg_str);
    free(path);
}

// Reply to request of file (it exists for sure)
void upload_file(int sock_fd, char *filename) {
    char *path = malloc(MAX_BUFFER_SIZE);
    char *msg_str = malloc(MAX_BUFFER_SIZE);

    sprintf(path, "%s/%s", FILES_LIBRARY_PATH, filename);
    int size = count_words(path);

    write_sync(sock_fd, &size, sizeof(int));

    FILE *f;
    f = fopen(path, "r");
    for (int i = 0; i < size; i++) {
        fscanf(f, "%s", msg_str);
        write_sync(sock_fd, msg_str, MAX_BUFFER_SIZE);
    }
    fclose(f);
    
    free(path);
    free(msg_str);
    free(filename);
}

// Reply to incoming connection
void * reply(void *arg) {
    int comm_fd = *(int*)(arg), msg_int;
    char* filename;

    read(comm_fd, &msg_int, MAX_BUFFER_SIZE);
    printf("[NEW REQUEST] Received request: %d\n", msg_int);
    
    switch (msg_int)
    {
        case SYNC_MSG_INT:
            receive_sync(comm_fd);
            break;

        case REQUEST_MSG_INT:
            filename = malloc(MAX_BUFFER_SIZE);
            memset(filename, 0, MAX_BUFFER_SIZE);
            read(comm_fd, filename, MAX_BUFFER_SIZE);   
            upload_file(comm_fd, filename);
            break;

        default:
            printf("[ERROR] Could not recognize received message: %d\n", msg_int);
            break;
    }

    close(comm_fd);
    return NULL;
}

// Wait for incoming TCP connection and reply, depending on command received
void wait_requests() {
    int master_sock_fd = 0, comm_sock_fd = 0, addr_len = 0;
    struct sockaddr_in server_addr, client_addr;

    addr_len = sizeof(client_addr);
    master_sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(MY_PORT));
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(master_sock_fd, (struct sockaddr *) &server_addr, sizeof(server_addr));

    listen(master_sock_fd, 20);

    while (1) {
        memset(&client_addr, 0, sizeof(client_addr));
        comm_sock_fd = accept(master_sock_fd, (struct sockaddr *) &client_addr, &addr_len);

        pthread_t new_thread;
        pthread_create(&new_thread, NULL, reply, &comm_sock_fd);
    }
}

// Sync with the first node (someone in network)
void first_sync(char *ip_port) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    int msg_int = SYNC_MSG_INT;
    char *msg_str = malloc(MAX_BUFFER_SIZE);
    char *ip = malloc(IP_PORT_SIZE);
    char *port = malloc(IP_PORT_SIZE);
    struct sockaddr_in server_addr;

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

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(port));
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    stringify_me(msg_str, network_name, network_ip);
    write_sync(sock_fd, &msg_int, sizeof(int));
    write_sync(sock_fd, msg_str, MAX_BUFFER_SIZE);
    write_sync(sock_fd, (int*)&nodes->count, sizeof(int));

    free(ip);
    free(port);

    close(sock_fd);
}


////// MAIN //////

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: ./node [Your IP]\n");
        return 0;
    }

    printf("|------| Faraday P2P Node v3.0 |------|\n\n");

    printf("Your network name: ");
    scanf("%s", network_name);
    network_ip = argv[1];

    char answer = '0';
    while (answer != 'y' && answer != 'n') {
        printf("Are you first node in network? [y/n] ");
        scanf(" %c", &answer);
        getchar();
    }

    nodes = create_array_list(STARTING_ARRAY_LIST_SIZE);

    pthread_t send_sync_thread, file_download_thread;

    // Ask specified ip for list of connected nodes
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
