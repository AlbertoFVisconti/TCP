#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

int set_ipv4_address(struct sockaddr_in * addr, const char * addr_s, uint16_t port) {
    /* Convert the address of the client to a string */
    memset(addr, 0, sizeof(*addr));
    if (inet_pton(AF_INET, addr_s, &addr->sin_addr) != 1) {
	fprintf(stderr, "invalid IPv4 address: %s\n", addr_s);
	return -1;
    }
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    return 0;
}

int verbose = 0;

const char * snd_addr_s;
uint16_t snd_port = 3456;

const char * rcv_addr_s;
uint16_t rcv_port = 6543;

int process_args (int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
	if (strncmp(argv[i], "ra=", 3) == 0) {
	    rcv_addr_s = argv[i] + 3;
	} else if (strncmp(argv[i], "rp=", 3) == 0) {
	    rcv_port = strtoul(argv[i] + 3, 0, 10);
	} else if (strncmp(argv[i], "sa=", 3) == 0) {
	    snd_addr_s = argv[i] + 3;
	} else if (strncmp(argv[i], "sp=", 3) == 0) {
	    snd_port = strtoul(argv[i] + 3, 0, 10);
	} else if (strcmp(argv[i], "-v") == 0) {
	    verbose = 1;
	} else {
	    fprintf(stderr,
                    "usage: %s [options]\n"
		    "options:\n"
		    "\tsa=<sender-addr>]\t\tIPv4 address of the sender\n"
		    "\tra=<receiver-addr>]\t\tIPv4 address of the receiver\n"
		    "\tsp=<sender-port>]\t\tport number of the sender (defailt 3456)\n"
		    "\trp=<receiver-port>]\t\tport number of the receiver (defailt 6543)\n"
		    "\t-v\t\t\t\t log transport-level actions onto standard error.\n",
		    argv[0]);
	    return -1;
	}
    }
    return 0;
}

