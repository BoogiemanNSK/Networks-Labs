#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <memory.h>
#include "common.h"

#define DEST_PORT            2000
#define SERVER_IP_ADDRESS    "192.168.1.2"

struct test_struct client_data;
struct result_struct result;

void setup_udp_communication() {
    /* Step 1: Initialization */
    int sockfd = 0,
        sent_recv_bytes = 0,
	addr_len = 0;

    addr_len = sizeof(struct sockaddr);
    struct sockaddr_in dest;

    /* Step 2: specify server information */
    /* IPv4 sockets, Other values are IPv6 */
    struct hostent *host = (struct hostent *)gethostbyname(SERVER_IP_ADDRESS);
    dest.sin_family = AF_INET;
    dest.sin_port = htons(DEST_PORT);
    dest.sin_addr = *((struct in_addr *)host->h_addr);

    /* Step 3: Create a TCP socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    /* Step 4: Get the data to be sent to server */
    while(1) {
	    printf("Enter student name : ");
	    scanf("%s", &client_data.s_name[0]);
	    printf("Enter studen age : ");
	    scanf("%u", &client_data.s_age);
	    printf("Enter student group name: ");
	    scanf("%s", &client_data.s_group_name[0]);

	    /* Step 5: Send the data to server */
	    sent_recv_bytes = sendto(sockfd,
		   &client_data,
		   sizeof(struct test_struct),
		   MSG_CONFIRM,
		   (struct sockaddr *)&dest,
		   sizeof(struct sockaddr));
	    printf("No of bytes sent = %d\n", sent_recv_bytes);

	    /* Step 6: Client also wants a reply from server after sending data */
	    sent_recv_bytes =  recvfrom(sockfd,
		   (char *)&result,
		   sizeof(struct result_struct),
 		   MSG_WAITALL,
		   (struct sockaddr *)&dest,
		   &addr_len);
	    printf("No of bytes received = %d\n", sent_recv_bytes);

	    printf("Result received = %s\n", result.s_info);
    }
}


int main(int argc, char **argv) {
    setup_udp_communication();
    printf("Application quits...\n");
    return 0;
}
