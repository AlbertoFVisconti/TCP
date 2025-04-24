CFLAGS = -Wall -g

IMPL = x

.PHONY: all
all: sender receiver

SND_OBJS = sender.o common.o sender_$(IMPL).o
sender: $(SND_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SND_OBJS)

RCV_OBJS = receiver.o common.o receiver_$(IMPL).o
receiver: $(RCV_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(RCV_OBJS)

.PHONY: clean
clean:
	rm -f sender receiver *.o
