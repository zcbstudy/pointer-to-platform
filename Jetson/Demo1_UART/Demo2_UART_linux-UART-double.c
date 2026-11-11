#include <stdio.h>
#include <stddef.h>
#include <pthread.h>

#define RX_SIZE 256

//typedef 是为了这个结构体定义一个别名 新名字在末尾存在
typedef struct 
{
    /* data */
    int fd;
    // 缓冲区暂存字节
    unsigned char rx_buf[RX_SIZE];
    // 当前缓冲区有效的字符数
    // size_t 是 C 语言专门用于表示对象大小、内存容量及相关字节数量的无符号整数类型。
    size_t rx_len;
    // 保护缓冲区状态
    pthread_mutex_t rx_mutex;
    // 收到新数据通知等待进程
    pthread_cond_t rx_cond;
    // 推出请求标志
    int stop;

}UartContext;

void *uart_rx_thread(void *arg)
{
    UartContext *ctx = (UartContext *)arg;

    return NULL;
}