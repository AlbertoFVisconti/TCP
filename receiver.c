#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "common.h"
#include "receiver.h"

int main(int argc, char *argv[]) {
    rcv_addr_s = "0.0.0.0";	/* ANY address for us (receiver) */
    snd_addr_s = "127.0.0.1";

    if (process_args(argc, argv) < 0)
	return EXIT_FAILURE;

    struct sockaddr_in snd_addr;
    struct sockaddr_in rcv_addr;

    if (set_ipv4_address(&rcv_addr, rcv_addr_s, rcv_port) == -1)
	return EXIT_FAILURE;
    if (set_ipv4_address(&snd_addr, snd_addr_s, snd_port) == -1)
	return EXIT_FAILURE;

    /* Create the UDP socket. */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
	perror("failed to create server socket");
	return EXIT_FAILURE;
    }
    /* Set the local address for the socket */
    if (bind(sockfd, (struct sockaddr *) &rcv_addr, sizeof(rcv_addr)) < 0) {
	perror("failed to bind socket");
	goto error_handling;
    }
    /* Set the remote address for the socket */
    if (connect(sockfd, (struct sockaddr *) &snd_addr, sizeof(snd_addr)) < 0) {
	perror("failed to connect socket");
	goto error_handling;
    }

    if (receiver(sockfd) < 0)
	goto error_handling;

    close(sockfd);
    return EXIT_SUCCESS;

 error_handling:
    close(sockfd);
    return EXIT_FAILURE;
}

