#include <stdio.h>
#include "stack.h"

int main(void)
{
    struct stack st;
    int value;
    int ret;

    ret = stack_init(&st);
    if (ret != STACK_OK)
    {
        print_error(ret);
        return ret;
    }

    ret = stack_push(&st, 10);
    if (ret != STACK_OK)
    {
        print_error(ret);
        return ret;
    }

    ret = stack_pop(&st, &value);
    if (ret != STACK_OK)
    {
        print_error(ret);
        return ret;
    }

    printf("pop value: %d\n", value);

    return 0;
}