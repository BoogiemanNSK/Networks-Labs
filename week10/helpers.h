#include <stdio.h>
#include <dirent.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "alist.h"

// PORT
#define MY_PORT "12000"

// COMMANDS
#define SYNC_MSG_INT 1
#define REQUEST_MSG_INT 0

// FIXED SIZES
#define NAME_SIZE 32
#define IP_PORT_SIZE 16
#define MAX_BUFFER_SIZE 1024
#define STARTING_ARRAY_LIST_SIZE 16

// STRINGS
#define FILES_LIBRARY_PATH "Shared"

// Structure that stores nodes info
struct node {
    char name[NAME_SIZE];
    char ip[IP_PORT_SIZE];
    char port[IP_PORT_SIZE];

    char **files;
};

// Structure that stores info about node which is currently processed (or was processed <5 ticks ago)
struct cdb_entry {
    int hash;
    int time;
};

char **get_local_files();
char get_answer(char *question);

int write_sync(int sock_fd, void *data, int data_size);
int check_local_file(char *path);
int count_words(char *path);
int blacklisted(p_array_list bdb, int hash, int *B_LOCK);
int calculate_hash(struct sockaddr_in *client_addr);
int current_list_time(p_array_list cdb, int hash, int *C_LOCK);

void increment_time(p_array_list cdb, p_array_list bdb, int hash, int *C_LOCK, int *B_LOCK);
void decrement_time(p_array_list cdb, int hash, int *C_LOCK);
void read_until(int sock, char* buf, size_t bytes);
void lock(int *LOCK);
void unlock(int *LOCK);
void wait_lock(const int *LOCK);
void rewrite_files(struct node *p, const char *msg);
void stringify_me(char *str, char *name, char *ip);
void stringify_node(char *str, struct node *p);

struct node *get_node_by_string(const char *str, char *network_name, p_array_list nodes);