// 头文件负责三件事：
// 定义 RX_SIZE
// 定义 UartContext 结构体类型
// 声明供其他 .c 文件调用的函数

//头文件保护
#ifndef UART_H
#define UART_H
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

typedef enum
{
    UART_OK = 0,
    UART_ERR_OPEN = -1,
    UART_ERR_CONFIG = -2,
    UART_ERR_MUTEX = -3,
    UART_ERR_COND = -4,
    UART_ERR_PARAM = -5
} UartResult;

// UART 配置
int uart_configure(int fd);

// UART 初始化
UartResult uart_init(UartContext *ctx, const char *device);

// UART 接收线程
void *uart_rx_thread(void *arg);

// UART 发送线程
void *uart_tx_thread(void *arg);

// UART 资源清理
void uart_destroy(UartContext *ctx);

#endif