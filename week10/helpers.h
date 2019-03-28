#include <stdio.h>
#include <dirent.h>

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

// Struct that stores nodes info
struct node {
    char name[NAME_SIZE];
    char ip[IP_PORT_SIZE];
    char port[IP_PORT_SIZE];

    char **files;
};

// Functions
char **get_local_files();
char get_answer(char *question);
int write_sync(int sockfd, void *data, int data_size);
int check_local_file(char *path);
int count_words(char *path);
void rewrite_files(struct node *p, char *msg);
void stringify_me(char *str, char *name, char *ip);
void stringify_node(char *str, struct node *p);
struct node *get_node_by_string(char *str, char *network_name, p_array_list nodes);