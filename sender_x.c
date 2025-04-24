#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <poll.h>
#include <time.h>
#include <assert.h>

#include "sender.h"
#include "common.h"

#include "gbn.h"

#define MAX_WINDOW  1024

enum cc_state {
    CC_SLOW_START,
    CC_CONGESTION_AVOIDANCE,
    CC_FAST_RECOVERY,
};

enum sender_state {
    OPEN,
    CLOSING,
    CLOSED
};

struct packet {
    size_t size;
    unsigned char bytes[GBN_HEADER + GBN_MSS];
};


int timeout_ms = 1000;

/* RTT estimation, all values are in milliseconds */
double rtt = 150.0;		/* current estimate of the RTT */
double rtt_dev = 50.0;		/* current estimate of the dev-RTT */
uint32_t rtt_expected_ack = 0;
struct timespec rtt_start;

int sstresh = 64;
int cwnd = 1;
int dup_ack_count = 0;
enum cc_state cc_state = CC_SLOW_START;
enum sender_state sock_state = OPEN;

uint32_t base = 0;
uint32_t next_seq = 0;


void rtt_timeout_event () {
    rtt_expected_ack = 0;
    timeout_ms *= 2;
}

void cc_timeout (void) {
	cc_state=CC_SLOW_START;
	sstresh=(cwnd/2>0)?cwnd/2 : 1;
	cwnd=1;
	dup_ack_count=0;
}

void cc_received_dup_ack (void) {
	switch (cc_state)
	{
	case CC_SLOW_START:
		dup_ack_count++;
		if(3==dup_ack_count){
			cc_state=CC_FAST_RECOVERY;
			sstresh=(cwnd/2>0)?cwnd/2 : 1;
			cwnd=sstresh+3;
		}
		break;
	
	case CC_FAST_RECOVERY:
		cwnd++;
		break;
	case CC_CONGESTION_AVOIDANCE:
		dup_ack_count++;
		if(3==dup_ack_count){
			cc_state=CC_FAST_RECOVERY;
			sstresh=(cwnd/2>0)?cwnd/2 : 1;
			cwnd=sstresh+3;
		}
		break;
	}
}

void cc_receive_acks (int acks) {
    switch (cc_state)
	{
	case CC_FAST_RECOVERY:
		cc_state=CC_CONGESTION_AVOIDANCE;
		dup_ack_count=0;
		//TODO understand why in book cwnd=sstresh however this violates the assertion next_seq < base + cwnd
		break;
	case CC_SLOW_START:
		dup_ack_count=0;
		cwnd+=acks;
		if(cwnd>=sstresh){
			cc_state=CC_CONGESTION_AVOIDANCE;
		}
		break;
	case CC_CONGESTION_AVOIDANCE:
		dup_ack_count=0;
		cwnd+= acks/cwnd;
		break;
	}
}

struct packet * get_pkt_buf (uint32_t seq)
{
    static struct packet P[MAX_WINDOW];

    return &(P[seq%MAX_WINDOW]);
}


void rtt_segment_sent (uint32_t seq) {
    if (rtt_expected_ack == 0) {
	if (clock_gettime(CLOCK_MONOTONIC, &rtt_start) < 0) {
	    perror("failed to get current time");
	    return;
	}
	rtt_expected_ack = seq + 1;
    }
}

int rtt_timeout () {
    return (int)(rtt + 4*rtt_dev);
}

void rtt_ack_received (uint32_t seq) {
    if (seq < rtt_expected_ack)
	return;
    if (seq == rtt_expected_ack) {
	struct timespec now;
	if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
	    perror("failed to get current time");
	    return;
	}
	double current = 1000.0*(now.tv_sec - rtt_start.tv_sec)
			 + 1.0*(now.tv_nsec - rtt_start.tv_nsec)/1000000.0;
	double dev_current = (rtt > current) ? rtt - current : current - rtt;

	rtt = 0.875*rtt + 0.125*current;
	rtt_dev = 0.75*rtt_dev + 0.25*dev_current;
	timeout_ms = rtt_timeout();
    }
    rtt_expected_ack = 0;
}


int poll_stdin (void) {
    return sock_state == OPEN && next_seq < base + cwnd;
}

struct pollfd pollfds[2];

void init_event_loop(int sockfd) {
    pollfds[0].fd = sockfd;
    pollfds[0].events = POLLIN | POLLERR | POLLHUP;
    pollfds[1].fd = STDIN_FILENO;
    pollfds[1].events = POLLIN | POLLERR | POLLHUP;
}

struct timespec timer_start;
int timer_active = 0;
int start_timer() {
    if (clock_gettime (CLOCK_MONOTONIC, &timer_start) < 0) {
	perror("could not get current time");
	return -1;
    }
    timer_active = 1;
    return 0;
}

void stop_timer() {
    timer_active = 0;
}

/* Wait for a network packet, an application input, or both.
 * Return:
 *
 *   -1 on error
 *    0 on timeout
 *    1 on network packet event
 *    2 on application input event
 *    3 on both network packet and application input events
 */
enum event_type {
    EVENT_ERROR = -1,
    EVENT_TIMEOUT = 0,
    EVENT_PACKET = 1,
    EVENT_INPUT = 2
};

int event_loop() {
    int poll_timeout;
    if (timer_active) {
	struct timespec now;
	if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
	    perror("failed to get current time");
	    return -1;
	}
	int elapsed = 1000*(now.tv_sec - timer_start.tv_sec)
		      + (now.tv_nsec - timer_start.tv_nsec)/1000000;
	if (elapsed >= timeout_ms)
	    return EVENT_TIMEOUT;
	poll_timeout = timeout_ms - elapsed;
    } else {
	poll_timeout = -1;
    }
    nfds_t nfds = poll_stdin() ? 2 : 1;
    int poll_res = poll(pollfds, nfds, poll_timeout);
    if (poll_res < 0) {
	perror("failure while polling to receive ack from receiver");
	return -1;
    }
    if (poll_res == 0)
	return EVENT_TIMEOUT;
    /* check socket first */
    if (pollfds[0].revents & (POLLERR | POLLHUP)) {
	fprintf(stderr, "errors on socket\n");
	return -1;
    }
    int res = 0;
    if (pollfds[0].revents & POLLIN)
	res |= EVENT_PACKET;
    if (poll_stdin()) {
	if (pollfds[1].revents & (POLLERR | POLLHUP)) {
	    fprintf(stderr, "errors on stdin\n");
	    return -1;
	}
	if (pollfds[1].revents & POLLIN)
	    res |= EVENT_INPUT;
    }
    return res;
}

int sender (int sockfd) {
    /* statistics and counters */
    size_t packets = 0;
    size_t acks = 0;
    size_t timeouts = 0;
    size_t segments = 0;
    size_t total_size = 0;


    init_event_loop(sockfd);

    while (sock_state != CLOSED) {
	if (verbose) {
	    fprintf(stderr, " base=%u  seg=%zu  size=%zu  pkt=%zu  ack=%zu  to=%zu  rtt=%lf\r",
		    base, segments, total_size, packets, acks, timeouts, rtt);
	    fflush(stderr);
	}

	int ev = event_loop();

	if (ev == EVENT_ERROR)
	    return -1;

	if (ev == EVENT_TIMEOUT) {  	/* retransmit the base packet */
	    ++timeouts;
	    rtt_timeout_event();
	    dup_ack_count = 0;
	    cc_timeout();
	    struct packet * pkt = get_pkt_buf(base);
	    if (send(sockfd, pkt->bytes, pkt->size, 0) < 0) {
		perror("failed to send data to the receiver");
		return -1;
	    }
	    rtt_segment_sent(base);
	    ++packets;
	    if (start_timer() < 0)
		return -1;
	    continue;
	}

	if (ev & EVENT_PACKET) {	/* reading ack */
	    unsigned char ack_pkt[GBN_HEADER];
	    ssize_t ack_res = recv(sockfd, ack_pkt, GBN_HEADER, 0);
	    if (ack_res < 0) {
		perror("failed to read ack packet");
		return -1;
	    }
	    if (ack_res < GBN_HEADER) {
		fprintf(stderr, "invalid ack packet\n");
	    } else {
		++acks;
		uint32_t ack_seq = gbn_get_seq(ack_pkt);
		rtt_ack_received(ack_seq);

		if (ack_seq == base) {
		    cc_received_dup_ack();
		    if (dup_ack_count >= 3) {
		        dup_ack_count = 0;
			/* fast retransmission */
			struct packet * pkt = get_pkt_buf(base);
			if (send(sockfd, pkt->bytes, pkt->size, 0) < 0) {
			    perror("failed to send data to the receiver");
			    return -1;
			}
			rtt_segment_sent(base);
			++packets;
			if (start_timer() < 0)
			    return -1;
		    }
		} else if (ack_seq > base && ack_seq <= next_seq) {
		    dup_ack_count = 0;
		    cc_receive_acks(ack_seq - base);
		    base = ack_seq;
		    if (sock_state == CLOSING) {
			if (base != next_seq) {
			    if (start_timer() < 0)
				return -1;
			} else {
			    sock_state = CLOSED;
			    stop_timer();
			}
		    } else {
			if (base != next_seq) {
			    if (start_timer() < 0)
				return -1;
			} else
			    stop_timer();
		    }
		}
	    }
	}

	if (ev & EVENT_INPUT) {
	    assert(next_seq < base + cwnd);
	    struct packet * pkt = get_pkt_buf(next_seq);
	    gbn_set_seq(pkt->bytes, next_seq);
	    ssize_t seg_len = read(STDIN_FILENO, pkt->bytes + GBN_HEADER, GBN_MSS);
	    if (seg_len < 0) {
		perror("failed to read data from the standard input");
		return -1;
	    }
	    pkt->size = GBN_HEADER + seg_len;
	    ++segments;
	    total_size += seg_len;
	    if (send(sockfd, pkt->bytes, pkt->size, 0) < 0) {
		perror("failed to send data to the receiver");
		return -1;
	    }
	    ++packets;
	    rtt_segment_sent(next_seq);
	    if (seg_len == 0) {
		sock_state = CLOSING;
	    }
	    if (next_seq == base)
		if (start_timer() < 0)
		    return -1;
	    ++next_seq;
	}
    }
    if (verbose)
	fprintf(stderr, "\n");
    return 0;
}
