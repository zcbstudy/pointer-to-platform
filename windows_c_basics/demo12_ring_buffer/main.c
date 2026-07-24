#include <stdio.h>
#include "ringbuffer.h"

int main(void)
{
    struct RingBuffer rb;
    int ret;
    int value;

    ret = rb_init(&rb);
    print_error(ret);

    ret = rb_push(&rb, 10);
    print_error(ret);

    ret = rb_push(&rb, 20);
    print_error(ret);

    ret = rb_pop(&rb, &value);
    print_error(ret);
    if (ret == RB_OK)
    {
        printf("value = %d\n", value);
    }

    ret = rb_pop(&rb, &value);
    print_error(ret);
    if (ret == RB_OK)
    {
        printf("value = %d\n", value);
    }

    ret = rb_pop(&rb, &value);
    print_error(ret);

    return 0;
}