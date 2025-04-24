#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

#include <netinet/in.h>
#include <stdint.h>

extern int set_ipv4_address(struct sockaddr_in * addr, const char * addr_s, uint16_t port);

extern int verbose;

extern const char * snd_addr_s;
extern uint16_t snd_port;

extern const char * rcv_addr_s;
extern uint16_t rcv_port;

extern int process_args (int argc, char *argv[]);

#define MAX_SEGMENT_SIZE 1000

#endif

