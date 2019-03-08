#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <memory.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_PORT     2000

char data_buffer[1024];

struct message_struct {
    char data[256];
};

void setup_tcp_server_communication() {
    int master_sock_tcp_fd = 0,
            sent_recv_bytes = 0,
            addr_len = 0,
            opt = 1;

    fd_set readfds;
    int comm_socket_fd = 0;
    struct sockaddr_in server_addr, client_addr;

    if ((master_sock_tcp_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        printf("Master socket creation failed.\n");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = SERVER_PORT;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    addr_len = sizeof(struct sockaddr);

    if (bind(master_sock_tcp_fd, (struct sockaddr *) &server_addr, sizeof(struct sockaddr)) == -1) {
        printf("Master socket bind failed\n");
        return;
    }

    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(master_sock_tcp_fd, (struct sockaddr *)&sin, &len) == -1)
        perror("getsockname");
    else
        printf("port number %d\n", ntohs(sin.sin_port));

    if (listen(master_sock_tcp_fd, 5) < 0) {
        printf("Master socket listen failed\n");
        return;
    }

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(master_sock_tcp_fd, &readfds);

        select(master_sock_tcp_fd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(master_sock_tcp_fd, &readfds)) {

            comm_socket_fd = accept(master_sock_tcp_fd, (struct sockaddr *) &client_addr, &addr_len);
            if (comm_socket_fd < 0) {
                printf("accept error : errno = %d\n", errno);
                exit(0);
            }

            printf("Connection accepted from client : %s:%u\n",
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            while (1) {
                memset(data_buffer, 0, sizeof(data_buffer));

                sent_recv_bytes = recvfrom(comm_socket_fd, (char *) data_buffer, sizeof(data_buffer), 0,
                                           (struct sockaddr *) &client_addr, &addr_len);

                // TODO Compose handshake message.
                message_struct message;

                sent_recv_bytes = sendto(comm_socket_fd, (char *) &message, sizeof(message_struct), 0,
                                         (struct sockaddr *) &client_addr, sizeof(struct sockaddr));
            }
        }
    }
}

int main(int argc, char **argv) {
    setup_tcp_server_communication();
    return 0;
}