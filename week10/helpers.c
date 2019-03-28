#include "helpers.h"


// Returns an y/n answer
char get_answer(char *question) {
    char answer = '0';
    while (answer != 'y' && answer != 'n') {
        printf("%s ", question);
        scanf(" %c", &answer);
        getchar();
    }
    return answer;
}


// Synchronized version of write that waits 0.1 seconds before sending
int write_sync(int sockfd, void *data, int data_size) {
    int n = write(sockfd, data, data_size);
    usleep(100000);
    return n;
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
struct node *get_node_by_string(char *str, char *network_name, p_array_list nodes) {
    char *temp = malloc(NAME_SIZE);
    struct node *current = NULL;
    
    int t = 0;
    while (str[t] != ':') {
        temp[t] = str[t];
        t++;
    }
    temp[t] = '\0';

    if (strncmp(temp, network_name, strlen(temp)) == 0) {
        return current;
    }

    int exist = 0;
    for (int i = array_list_iter(nodes); i != -1; i = array_list_next(nodes, i)) {
        current = array_list_get(nodes, i);
        if (strncmp(temp, current->name, strlen(temp)) == 0) {
            exist = 1;
            break;
        }
    }

    if (exist == 0) {
        printf("Added new node - %s\n", temp);

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

        new_node->files = malloc(MAX_BUFFER_SIZE);
        new_node->files[0] = NULL;

        array_list_add(nodes, new_node);
        current = new_node;
    }

    free(temp);
    return current;
}


// Updates file library for given node
void rewrite_files(struct node *p, char *msg) {
    int i = 0, k = 0, t;
    for (int j = 0; j < 3; j++) {
        while (msg[i] != ':') { i++; }
        i++;
    }

    int add = 0;
    while(msg[i] != '\0' && msg[i] != '\r') {
        t = 0;

        if (add || p->files[k] == NULL) {
            p->files[k] = malloc(MAX_BUFFER_SIZE);
            add = 1;
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
    
    sprintf(str, "%s:%s:%s", name, ip, MY_PORT);

    if (files_lib[0] == NULL) {
        strcat(str, ":");
    }

    int k = 0;
    while (files_lib[k] != NULL) {
        strcat(str, ":");
        strcat(str, files_lib[k]);
        k++;
    }
}


// Converts info about node into string
void stringify_node(char *str, struct node *p) {
    sprintf(str, "%s:%s:%s:", p->name, p->ip, p->port);
}