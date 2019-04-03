#include "helpers.h"


// Returns an y/n answer
char get_answer(char *question) {
    char answer = '0';
    while (answer != 'y' && answer != 'n') {
        printf("%s", question);
        scanf(" %c", &answer);
        getchar();
    }
    return answer;
}


// Synchronized version of write that waits 0.1 seconds before sending
int write_sync(int sock_fd, void *data, int data_size) {
    int n = write(sock_fd, data, data_size);
    usleep(100000);
    return n;
}


// Checks if given hash is in blacklist
int blacklisted(p_array_list bdb, int hash, int *B_LOCK) {
    wait_lock(B_LOCK);
    lock(B_LOCK);
    
    int *b_hash;
    int i = array_list_iter(bdb);
    for (; i != -1; i = array_list_next(bdb, i)) {
        b_hash = (int*)array_list_get(bdb, i);
        if (*b_hash == hash) {
            unlock(B_LOCK);
            return 1;
        }
    }

    unlock(B_LOCK);
    return 0;
}


// Calculates hash from given ip
int calculate_hash(struct sockaddr_in *client_addr) {
    int hash = 0;
    char *ip_string = inet_ntoa(client_addr->sin_addr);
    
    for (int i = 0; i < strlen(ip_string); i++) {
        hash += ip_string[i];
    }

    return hash;
}


// Increases time value of given hash
void increment_time(p_array_list cdb, p_array_list bdb, int hash, int *C_LOCK, int *B_LOCK) {
    wait_lock(C_LOCK);
    lock(C_LOCK);
    
    struct cdb_entry *c_node;
    int i = array_list_iter(cdb);
    for (; i != -1; i = array_list_next(cdb, i)) {
        c_node = array_list_get(cdb, i);
        if (c_node->hash == hash) {
            c_node->time++;

            if (c_node->time > 5) {
                wait_lock(B_LOCK);
                lock(B_LOCK);
    
                int *b_hash = (int*)malloc(sizeof(int));
                *b_hash = hash;
                array_list_add(bdb, b_hash);
                array_list_remove(cdb, c_node);
                printf("Blacklisted hash %d\n", hash);

                unlock(B_LOCK);
            }

            unlock(C_LOCK);
            return;
        }
    }

    c_node = (struct cdb_entry *)malloc(sizeof(struct cdb_entry));
    c_node->hash = hash;
    c_node->time = 0;
    array_list_add(cdb, c_node);

    unlock(C_LOCK);
}


// Decreases time value of given hash
void decrement_time(p_array_list cdb, int hash, int *C_LOCK) {
    wait_lock(C_LOCK);
    lock(C_LOCK);
    
    struct cdb_entry *c_node;
    int i = array_list_iter(cdb);
    for (; i != -1; i = array_list_next(cdb, i)) {
        c_node = array_list_get(cdb, i);
        if (c_node->hash == hash) {
            c_node->time--;

            if (c_node->time < 0) {
                array_list_remove(cdb, c_node);
            }
            
            break;
        }
    }

    unlock(C_LOCK);
}


// Synchronized version of read that waits until 'bytes' amount of bytes is acquired
int read_until(int sock, char* buf, size_t bytes) {
    size_t count = 0;
    ssize_t received = 0;
    while((received = read(sock, buf, bytes-count)) > 0) {
        count += received;
        if (count == bytes) return count;
    }

    printf("[ERROR] SYNC: Could not receive everything\n");
    return -1;
}


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


// Returns names of files in local library
char **get_local_files() {
    DIR *d;
    struct dirent *dir;
    char **files = malloc(MAX_BUFFER_SIZE);
    int i = 0;

    d = opendir(FILES_LIBRARY_PATH);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if (dir->d_name[0] == '.') { continue; }
            files[i] = malloc(MAX_BUFFER_SIZE);
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
struct node *get_node_by_string(const char *str, char *network_name, p_array_list nodes) {
    char *temp = malloc(NAME_SIZE);
    
    int t = 0;
    while (str[t] != ':') {
        temp[t] = str[t];
        t++;
    }
    temp[t] = '\0';

    // Me
    if (strncmp(temp, network_name, strlen(temp)) == 0) {
        free(temp);
        return NULL;
    }

    // Exists
    struct node *current;
    for (int i = array_list_iter(nodes); i != -1; i = array_list_next(nodes, i)) {
        current = array_list_get(nodes, i);
        if (strncmp(temp, current->name, strlen(temp)) == 0) {
            free(temp);
            return current;
        }
    }

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

    new_node->files = malloc(MAX_BUFFER_SIZE * sizeof(char*));
    for (i = 0; i < MAX_BUFFER_SIZE; i++) {
        new_node->files[i] = NULL;
    }
    array_list_add(nodes, new_node);

    // New node
    printf("Added new node - %s\n", str);

    free(temp);
    return new_node;
}


// Waits one tick if lock is 0
void wait_lock(const int *LOCK) {
    while (*LOCK) { usleep(0); }
}


// Sets lock to 1
void lock(int *LOCK) {
    *LOCK = 1;
}


// Sets lock to 0
void unlock(int *LOCK) {
    *LOCK = 0;
}


// Updates file library for given node
void rewrite_files(struct node *p, const char *msg) {
    int i = 0, k = 0, t;
    for (int j = 0; j < 3; j++) {
        while (msg[i] != ':') { i++; }
        i++;
    }

    while(msg[i] != '\0' && msg[i] != '\r') {
        t = 0;
        i = (k == 0) ? i : (i + 1);

        // If more space should be reserved for files
        if (p->files[k] == NULL) {
            p->files[k] = malloc(MAX_BUFFER_SIZE);
        }

        while (msg[i] != ',' && msg[i] != '\0' && msg[i] != '\r') {
            p->files[k][t] = msg[i];
            t++; i++;
        }
        
        p->files[k][t] = '\0';
        k++;
    }

    p->files[k] = NULL;
}


// Converts info about 'me' into string str
void stringify_me(char *str, char *name, char *ip) {
    char **files_lib = get_local_files();
    
    sprintf(str, "%s:%s:%s:", name, ip, MY_PORT);

    if (files_lib[0] == NULL) {
        return;
    }

    int k = 0;
    while (files_lib[k + 1] != NULL) {
        strcat(str, files_lib[k]);
        strcat(str, ",");
        k++;
    }
    strcat(str, files_lib[k]);
}


// Converts info about node into string
void stringify_node(char *str, struct node *p) {
    sprintf(str, "%s:%s:%s:", p->name, p->ip, p->port);
}