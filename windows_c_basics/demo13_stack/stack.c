#include <stdio.h>
#include <stdlib.h>
#include "stack.h"

//初始化
int stack_init(struct stack* st)
{
	if (st == NULL)
	{
		return STACK_NULL;
	}
	st->top = 0;
	return STACK_OK;
}

//插入
int stack_push(struct stack* st, int value)
{
	if (st == NULL)
	{
		return STACK_NULL;
	}
	if (st->top == STACK_SIZE)
	{
		return STACK_FULL;
	}
	st->data[st->top] = value;
	st->top++;
	return STACK_OK;
}

//取出
int stack_pop(struct stack* st, int* value)
{
	if (st == NULL || value == NULL)
	{
		return STACK_NULL;
	}
	if (st->top == 0)
	{
		return STACK_EMPTY;
	}
	st->top--;
	*value = st->data[st->top];
	return STACK_OK;
}
//错误码
void print_error(int ret)
{
	switch (ret)
	{
	case STACK_OK:
		printf("stack is ok\n");
		break;
	case STACK_NULL:
		printf("stack is null\n");
		break;
	case STACK_FULL:
		printf("stack is full\n");
		break;
	case STACK_EMPTY:
		printf("stack is empty\n");
		break;
	default:
		break;
	}
}