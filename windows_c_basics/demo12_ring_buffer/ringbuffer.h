#ifndef RINGBUFFER_H
#define RINGBUFFER_H
#define BUFFER_SIZE 256
#define RB_OK 0
#define RB_NULL -1
#define RB_EMPTY -2

struct RingBuffer
{
	int data[BUFFER_SIZE];
	int head;
	int tail;
	int count;
};

int rb_push(struct RingBuffer* rb, int data);


#endif
