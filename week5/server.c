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
#include <pthread.h>
#include "common.h"

#define SERVER_PORT 2000
#define THREADS_NUM 8

/* To check threads avilibility */
int threads_mask = (1 + 2 + 4 + 8 + 16 + 32 + 64 + 128);
char data_buffer[1024];

struct thread_args {
	int thread_id;
	int sock_fd;
	int sent_recv_bytes;
	struct sockaddr_in *client_addr;
	struct test_struct *client_data;
};

void *handle_udp_request(void *args) {
	struct thread_args *info = (struct thread_args *)args;
	threads_mask -= (1 << info->thread_id);

	printf("Thread %d recvd %d bytes from client %s:%u\n", info->thread_id, info->sent_recv_bytes,
		inet_ntoa(info->client_addr->sin_addr), ntohs(info->client_addr->sin_port));

	if (info->sent_recv_bytes == 0) {
		close(info->sock_fd);

		printf("Thread %d went to sleep for 20sec...\n\n", info->thread_id);
		sleep(20);

		printf("\nThread %d is awake!\n\n", info->thread_id);
		threads_mask += (1 << info->thread_id);

		free(info->client_data);
		free(info);
		return;
	}

	if (info->client_data->s_age == 0) {
		close(info->sock_fd);

		printf("Thread %d went to sleep for 20sec...\n\n", info->thread_id);
		sleep(20);

		printf("\nThread %d is awake!\n\n", info->thread_id);
		threads_mask += (1 << info->thread_id);

		free(info->client_data);
		free(info);
		return;
	}

	///// DATA WRAPPER BEGIN /////
	struct result_struct result;

	result.s_info[0] = '{';
	int i = 1, j = 0;

	for (; info->client_data->s_name[j] != '\0'; i++, j++) {
		result.s_info[i] = info->client_data->s_name[j];
	}

	result.s_info[i++] = ',';
	result.s_info[i++] = ' ';

	char s_age[10];
	snprintf(s_age, 10, "%d", info->client_data->s_age);
	for (j = 0; s_age[j] >= '0' && s_age[j] <= '9'; i++, j++) {
		result.s_info[i] = s_age[j];
	}

	result.s_info[i++] = ',';
	result.s_info[i++] = ' ';

	for (j = 0; info->client_data->s_group_name[j] != '\0'; i++, j++) {
		result.s_info[i] = info->client_data->s_group_name[j];
	}

	result.s_info[i] = '}';
	///// DATA WRAPPER END /////

	printf("Received data: %s\n", result.s_info);

	/* Server replying back to client now */
	info->sent_recv_bytes = sendto(info->sock_fd, (char *)&result, sizeof(struct result_struct), MSG_CONFIRM,
		(struct sockaddr *) info->client_addr, sizeof(struct sockaddr));

	printf("Thread %d sent %d bytes in reply to client\n", info->thread_id, info->sent_recv_bytes);

	printf("Thread %d went to sleep for 20sec...\n\n", info->thread_id);
	sleep(20);

	printf("\nThread %d is awake!\n\n", info->thread_id);
	threads_mask += (1 << info->thread_id);

	free(info->client_data);
	free(info);
	return;
}

void setup_udp_server_communication() {

    /* Step 1 : Initialization */
    int sock_fd = 0,
	addr_len = 0,
	sent_recv_bytes = 0;
    struct sockaddr_in server_addr, client_addr;
	pthread_t threads[THREADS_NUM];

    /* Step 2: UDP socket creation */
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        printf("Socket creation failed\n");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));
    addr_len = sizeof(struct sockaddr);

    /* Step 3: Specify server Information */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    /* Step 4: Bind the server */
    if (bind(sock_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        printf("Socket bind failed\n");
        return;
    }

    while (1) {
	printf("\nServer ready to service new clients.\n\n");
        memset(data_buffer, 0, sizeof(data_buffer));

        /* Step 5: Trying to recieve data */
        sent_recv_bytes = recvfrom(sock_fd, (char *) data_buffer, sizeof(data_buffer), MSG_WAITALL,
                                       (struct sockaddr *) &client_addr, &addr_len);

		/* Look for free thread and use it to handle request */
		int i;
		for (i = 0; i < THREADS_NUM; i++) {
			if ((threads_mask & (1 << i)) > 0) {
				printf("Thread %d is not busy ==> Using it to handle request.\n", i);

				struct thread_args *t = (struct thread_args *)
					malloc(sizeof(struct thread_args));

				t->thread_id = i;
				t->sock_fd = sock_fd;
				t->sent_recv_bytes = sent_recv_bytes;
				t->client_addr = &client_addr;
				t->client_data = (struct test_struct *)
					malloc(sizeof(data_buffer));

				memcpy(t->client_data, data_buffer, sizeof(data_buffer));

				if (pthread_create(&threads[i], NULL, handle_udp_request, t)) {
					printf("Error creating thread\n");
					return;
				}

				break;
			}
			if (i == THREADS_NUM - 1) {
				printf("No threads are currently availible.\n");
			}
		}
    }
}

int main(int argc, char **argv) {
    setup_udp_server_communication();
    return 0;
}
