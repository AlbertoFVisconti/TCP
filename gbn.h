#ifndef GBN_H_INCLUDED
#define GBN_H_INCLUDED

#include <stdint.h>

#define GBN_MSS 1000

#define GBN_HEADER 4

static uint32_t gbn_get_seq(const unsigned char * pkt) {
    uint32_t s = pkt[0];
    for (unsigned i = 1; i < 4; ++i)
	s = (s << 8) | pkt[i];
    return s;
}

static void gbn_set_seq(unsigned char * pkt, uint32_t s) {
    pkt[0] = (s >> 24) & 0xff;
    pkt[1] = (s >> 16) & 0xff;
    pkt[2] = (s >> 8) & 0xff;
    pkt[3] = s & 0xff;
}

#endif
