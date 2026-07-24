#ifndef STACK_h
#define STACK_h
#define STACK_SIZE 256
#define STACK_OK 0
#define STACK_NULL -1
#define STACK_FULL -2
#define STACK_EMPTY -3

struct stack
{
	int data[STACK_SIZE];
	int top;
};

#endif