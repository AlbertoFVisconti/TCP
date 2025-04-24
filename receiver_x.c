#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "receiver.h"
#include "common.h"

#include "gbn.h"

struct packet {
    size_t size;		/* total size, including header */
    unsigned char bytes[GBN_HEADER + GBN_MSS];
    struct packet * next;	/* next available buffer, in free list */
};

#define PKT_BUF_SIZE 100

struct packet P_storage[PKT_BUF_SIZE];
struct packet * P[PKT_BUF_SIZE];
struct packet * free_packets = 0;

struct packet * allocate_packet_buffer() {
    assert(free_packets);
    struct packet * p = free_packets;
    free_packets = free_packets->next;
    return p;
}

void free_packet_buffer(struct packet * p) {
    p->next = free_packets;
    free_packets = p;
}

enum receiver_state {
    OPEN = 0,
    CLOSING = 1,
    CLOSED = 2
};

int receiver (int sockfd) {
    /* stats/counters */
    size_t packets = 0;
    size_t segments = 0;
    size_t seq_violations = 0;
    size_t total_size = 0;
    uint32_t expected_seq = 0;
    unsigned char ack_pkt[GBN_HEADER];

    for (int i = 0; i < PKT_BUF_SIZE; ++i) {
	P_storage[i].next = free_packets;
	free_packets = &(P_storage[i]);
    }
    for (int i = 0; i < PKT_BUF_SIZE; ++i)
	P[i] = 0;

    enum receiver_state state = OPEN;
    while (state == OPEN) {
	if (verbose) {
	    fprintf(stderr, " seg=%zu  size=%zu  pkt=%zu  seq_err=%zu\r",
		    segments, total_size, packets, seq_violations);
	    fflush(stderr);
	}
	/* extract packet buffer p from the free list */
	struct packet * p = allocate_packet_buffer(); 
	ssize_t pkt_len = recv(sockfd, p->bytes, GBN_HEADER + GBN_MSS, 0);
	if (pkt_len == 0) {
	    fprintf(stderr, "received invalid, zero-length packet from sender\n");
	    return -1;
	}
	if (pkt_len < 0) {
            perror("failed to receive data from sender");
            return -1;
	}
	if (pkt_len < GBN_HEADER) {
	    fprintf(stderr, "received invalid packet\n");
            return -1;
	}
	++packets;
	p->size = pkt_len;
	uint32_t seg_seq = gbn_get_seq(p->bytes);
	if (seg_seq != expected_seq) {
	    ++seq_violations;
	    gbn_set_seq(ack_pkt, expected_seq);
	    if (send(sockfd, ack_pkt, GBN_HEADER, 0) < 0) {
		perror("failed to send ack to sender");
		return -1;
	    }
	    if (seg_seq > expected_seq && free_packets != 0) {
		/* it's a seq number in the future, and we have space to
		   store the packet */
		P[seg_seq % PKT_BUF_SIZE] = p;
	    } else {
		free_packet_buffer(p);
	    }
	} else {		/* in-sequence segment */
	    do {
		++segments;
		size_t seg_len = p->size - GBN_HEADER;
		total_size += seg_len;
		/* deliver the data segment in p to the application */
		for (size_t written = 0; written < seg_len;) {
		    ssize_t w_res = write(STDOUT_FILENO, p->bytes + GBN_HEADER + written,
					  seg_len - written);
		    if (w_res < 0) {
			perror("failed to write to standard output");
			return -1;
		    }
		    written += w_res;
		}
		P[expected_seq % PKT_BUF_SIZE] = 0;
		/* check whether this was the last packet */
		if (seg_len == 0) 
		    state = CLOSING;
		/* recycle p */
		free_packet_buffer(p);
		++expected_seq;
		p = P[expected_seq % PKT_BUF_SIZE];
	    } while (p != 0);
	    gbn_set_seq(ack_pkt, expected_seq);
	    if (send(sockfd, ack_pkt, GBN_HEADER, 0) < 0) {
		perror("failed to send ack to sender");
		return -1;
	    }
	}
    }
    fprintf(stderr, "\n");
    return 0;
}

