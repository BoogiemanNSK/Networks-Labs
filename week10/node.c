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
p_array_list kdb, cdb, bdb;
char network_name[NAME_SIZE];
char* network_ip;
int KDB_LOCK, CDB_LOCK, BDB_LOCK;


// Send list of known nodes to all nodes
void * send_sync() {
    int sock_fd;
    char *temp;

    int msg_int;
    char *msg_str = malloc(MAX_BUFFER_SIZE);

    struct node *current, *cur;
    struct sockaddr_in server_addr;

    while(1) {
        sleep(5);

        for (int i = array_list_iter(kdb); i != -1; i = array_list_next(kdb, i)) {

            current = array_list_get(kdb, i);

            sock_fd = socket(AF_INET, SOCK_STREAM, 0);

            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(strtol(current->port, &temp, 10));
            inet_pton(AF_INET, current->ip, &server_addr.sin_addr);

            if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
                printf("[ERROR] Couldn't connect to %s\n", current->name);
                continue;
            }

            stringify_me(msg_str, network_name, network_ip);

            msg_int = htonl(SYNC_MSG_INT);
            write_sync(sock_fd, &msg_int, sizeof(int));
            write_sync(sock_fd, msg_str, MAX_BUFFER_SIZE);

            msg_int = htonl((int)kdb->count);
            write_sync(sock_fd, &msg_int, sizeof(int));

            for (int p = array_list_iter(kdb); p != -1; p = array_list_next(kdb, p)) {
                cur = array_list_get(kdb, p);

                stringify_node(msg_str, cur);
                write_sync(sock_fd, msg_str, MAX_BUFFER_SIZE);
            }

            close(sock_fd);
        }
    }
}


// On receiving sync, compare your list and received
void receive_sync(int comm_fd) {
    int msg_int, msg_len;
    char *msg_str = (char*)malloc(MAX_BUFFER_SIZE);
    
    struct node *current;
    memset(msg_str, 0, MAX_BUFFER_SIZE);
    msg_len = read_until(comm_fd, msg_str, MAX_BUFFER_SIZE);
    if (msg_len == -1) {
        free(msg_str);
        return;
    }

    wait_lock(&KDB_LOCK);
    lock(&KDB_LOCK);

    current = get_node_by_string(msg_str, network_name, kdb);
    rewrite_files(current, msg_str);

    read(comm_fd, &msg_int, sizeof(int));
    msg_int = ntohl(msg_int);
    for (int i = 0; i < msg_int; i++) {
        memset(msg_str, 0, MAX_BUFFER_SIZE);
        read_until(comm_fd, msg_str, MAX_BUFFER_SIZE);
        get_node_by_string(msg_str, network_name, kdb);
    }

    unlock(&KDB_LOCK);

    free(msg_str);
}


// Ask all known kdb for file and download it
void * send_request() {
    int sock_fd = 0, i, msg_int = REQUEST_MSG_INT;
    struct sockaddr_in server_addr;
    struct node *current = NULL;
    char *temp;

    if (get_answer("Do you want to download any files? [y/n]\n") == 'n') {
        return NULL;
    }

    char *path = malloc(MAX_BUFFER_SIZE);
    char *msg_str = malloc(MAX_BUFFER_SIZE);
    char *filename = malloc(MAX_BUFFER_SIZE);

    printf("[KNOWN FILES]\n");
    i = array_list_iter(kdb);
    for (; i != -1; i = array_list_next(kdb, i)) {
        current = array_list_get(kdb, i);

        int j = 0;
        while (current->files[j] != NULL) {
            printf("%s\n", current->files[j]);
            j++;
        }
    }

    printf("Specify filename:\n");
    scanf("%s", filename);
    
    if (check_local_file(filename) == 1) {
        printf("[ERR] You already have that file\n");
        return NULL;
    }

    i = array_list_iter(kdb);
    int found = 0;
    for (; i != -1; i = array_list_next(kdb, i)) {
        current = array_list_get(kdb, i);

        int j = 0;
        while (current->files[j] != NULL) {
            if (strcmp(filename, current->files[j]) == 0) {
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
        printf("[ERR] No such file exist across known kdb libraries\n");
        return NULL;
    }

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(strtol(current->port, &temp, 10));
    inet_pton(AF_INET, current->ip, &server_addr.sin_addr);

    connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    msg_int = htonl(msg_int);
	write_sync(sock_fd, &msg_int, sizeof(int));
    write_sync(sock_fd, filename, MAX_BUFFER_SIZE);
    
    read(sock_fd, &msg_int, sizeof(int));
    msg_int = ntohl(msg_int);

    sprintf(path, "%s/%s", FILES_LIBRARY_PATH, filename);

    FILE *fp = fopen(path, "ab+");

    for (i = 0; i < msg_int; i++) {
        read_until(sock_fd, msg_str, MAX_BUFFER_SIZE);
        fprintf(fp, "%s ", msg_str);
    }
    fclose(fp);

    printf("Successfully downloaded %s\n", filename);

    free(filename);
    free(msg_str);
    free(path);
    close(sock_fd);

    return NULL;
}

// Reply to request of file (it exists for sure)
void upload_file(int sock_fd, char *filename) {
    char *path = malloc(MAX_BUFFER_SIZE);
    char *msg_str = malloc(MAX_BUFFER_SIZE);

    sprintf(path, "%s/%s", FILES_LIBRARY_PATH, filename);
    int size = count_words(path);

    size = htonl(size);
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
void * reply(void *args) {
    int comm_fd = ((int*)args)[0];
    int hash = ((int*)args)[1];
    int msg_int = 0;
    char* filename;

    read(comm_fd, &msg_int, sizeof(int));
    msg_int = ntohl(msg_int);

    switch (msg_int)
    {
        case SYNC_MSG_INT:
            receive_sync(comm_fd);
            break;

        case REQUEST_MSG_INT:
            filename = malloc(MAX_BUFFER_SIZE);
            memset(filename, 0, MAX_BUFFER_SIZE);
            read_until(comm_fd, filename, MAX_BUFFER_SIZE);
            upload_file(comm_fd, filename);
            break;

        default:
            printf("[ERROR] Could not recognize received message: %d\n", msg_int);
            break;
    }

    decrement_time(cdb, hash, &CDB_LOCK);

    close(comm_fd);
    free(args);
    return NULL;
}

// Wait for incoming TCP connection and reply, depending on command received
void wait_requests() {
    int master_sock_fd = 0, comm_sock_fd = 0;
    unsigned int addr_len;
    struct sockaddr_in server_addr, client_addr;
    char *temp;

    addr_len = sizeof(client_addr);
    master_sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(strtol(MY_PORT, &temp, 10));
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(master_sock_fd, (struct sockaddr *) &server_addr, sizeof(server_addr));

    listen(master_sock_fd, 5);

    while (1) {
        memset(&client_addr, 0, sizeof(client_addr));
        comm_sock_fd = accept(master_sock_fd, (struct sockaddr *) &client_addr, &addr_len);

        int hash = calculate_hash(&client_addr);
        if (blacklisted(bdb, hash, &BDB_LOCK)) {
            continue;
        }

        increment_time(cdb, bdb, hash, &CDB_LOCK, &BDB_LOCK);

        int *args = (int*)malloc(sizeof(int) * 2);
        args[0] = comm_sock_fd;
        args[1] = hash;

        pthread_t new_thread;
        pthread_create(&new_thread, NULL, reply, args);
    }
}

// Sync with the first node (someone in network)
int first_sync(const char *ip_port) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    int msg_int = SYNC_MSG_INT;
    char *msg_str = malloc(MAX_BUFFER_SIZE);
    char *ip = malloc(IP_PORT_SIZE);
    char *port = malloc(IP_PORT_SIZE);
    struct sockaddr_in server_addr;
    char *temp;

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
    server_addr.sin_port = htons(strtol(port, &temp, 10));
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    int res = connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    stringify_me(msg_str, network_name, network_ip);

    msg_int = htonl(msg_int);
    write_sync(sock_fd, &msg_int, sizeof(int));
    write_sync(sock_fd, msg_str, MAX_BUFFER_SIZE);
    
    msg_int = htonl(0);
    write_sync(sock_fd, &msg_int, sizeof(int));

    free(ip);
    free(port);
    close(sock_fd);

    return res;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: ./node [Your IP]\n");
        return 0;
    }

    printf("|------| Faraday P2P Node v4.0 |------|\n\n");

    printf("Your network name:\n");
    scanf("%s", network_name);
    network_ip = argv[1];

    char answer = '0';
    while (answer != 'y' && answer != 'n') {
        printf("Are you first node in network? [y/n]\n");
        scanf(" %c", &answer);
        getchar();
    }

    kdb = create_array_list(STARTING_ARRAY_LIST_SIZE);
    cdb = create_array_list(STARTING_ARRAY_LIST_SIZE);
    bdb = create_array_list(STARTING_ARRAY_LIST_SIZE);

    KDB_LOCK = 0;
    CDB_LOCK = 0;
    BDB_LOCK = 0;

    pthread_t send_sync_thread, file_download_thread;

    // Ask specified ip for list of connected kdb
    if (answer == 'n') {
        char input[IP_PORT_SIZE * 2];
        printf("Please, specify IP:PORT of node inside network: ");
        scanf("%s", input);
        int check = first_sync(input);
        if (check == -1) {
            printf("Failed to connect!\n");
            return errno;
        }
    }

    // Create PING/PONG loops
    pthread_create(&send_sync_thread, NULL, send_sync, NULL);
    pthread_create(&file_download_thread, NULL, send_request, NULL);

    // Listen for new connections
    printf("Listening for new connections on %s...\n", MY_PORT);
    wait_requests();
    return 0;
}
