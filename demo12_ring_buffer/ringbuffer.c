#include <stdio.h>
#include <stdlib.h>
#include "ringbuffer.h"

//初始化
int rb_init(struct RingBuffer* rb)
{
	if (rb == NULL)
	{
		return RB_NULL;
	}
	rb->head = 0;
	rb->tail = 0;
	rb->count = 0;
	return RB_OK;
}

//插入
int rb_push(struct RingBuffer* rb, int value)
{
	if (rb == NULL)
	{
		return RB_NULL;
	}
	if (rb->count == BUFFER_SIZE)
	{
		rb->data[rb->tail] = value;
		rb->tail = (rb->tail + 1) % BUFFER_SIZE;
		rb->head = (rb->head + 1) % BUFFER_SIZE;
	}
	else
	{
		rb->data[rb->tail] = value;
		rb->tail = (rb->tail + 1) % BUFFER_SIZE;
		rb->count++;
	}
	return RB_OK;
}

//取数
int rb_pop(struct RingBuffer* rb, int* value)
{
	if (rb == NULL || value == NULL)
	{
		return RB_NULL;
	}
	if (rb->count == 0)
	{
		return RB_EMPTY;
	}
	*value = rb->data[rb->head];
	rb->head = (rb->head + 1) % BUFFER_SIZE;
	rb->count--;
	return RB_OK;
}

//错误码封装
void print_error(int ret)
{
	switch (ret)
	{
	case RB_OK:
		printf("Operation successful\n");
		break;
	case RB_NULL:
		printf("Error: Ring buffer is NULL\n");
		break;
	case RB_EMPTY:
		printf("Error: Ring buffer is empty\n");
		break;
	}
}