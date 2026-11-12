#include "uart.h"

#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int uart_configure(int fd)
{
	struct termios tty;
	
	if(tcgetattr(fd,&tty) == -1)
	{
		perror("tcgetattr");
		return -1;
	}
	
	if(cfsetispeed(&tty, B115200) == -1)
	{
		perror("cfsetispeed");
		return -1;
	}

	if(cfsetospeed(&tty, B115200) == -1)
	{
		perror("cfsetospeed");
		return -1;
	}

	//配置8N1,启用接受见笔记
	tty.c_cflag &= ~(CSIZE | PARENB | CSTOPB);
	tty.c_cflag |= (CS8 | CREAD | CLOCAL);
	// 关闭硬件流控
    #ifdef CRTSCTS
    tty.c_cflag &= ~CRTSCTS;
    #endif
	//关不不要的
	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK |
			ISTRIP | INLCR | IGNCR |
			ICRNL | IXON | IXOFF | IXANY);
    //关闭输出后处理
	tty.c_oflag &= ~OPOST;
	//关闭规范模式
	tty.c_lflag &= ~(ECHO | ECHONL | ICANON |
			ISIG | IEXTEN);
	//设置read等配置
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 0;
	//应用
	if(tcsetattr(fd, TCSANOW,&tty) == -1)
	{
		perror("tcsetattr");
		return -1;
	}

	return 0; 
}

UartResult uart_init(UartContext *ctx, const char *device)
{
	//检查参数
    if(ctx == NULL || device == NULL)
    {
    	return UART_ERR_PARAM;
    }
    
    // 初始化结构体
    *ctx = (UartContext){0};
    ctx->fd = -1;
    
    // 打开串口
    int fd = open(device,O_RDWR|O_NOCTTY);
    
    if(fd == -1)
    {
		perror("fd打开失败");
        return UART_ERR_OPEN;
    }
    
    // 配置串口
    int configure = uart_configure(fd);
    if(configure == -1)
    {
        if(close(fd) == -1)
        {
            perror("close");
            return UART_ERR_CLOSE;
        }
        return UART_ERR_CONFIG;
    }
				
	// 初始化mutex
	if(pthread_mutex_init(&ctx->rx_mutex, NULL) != 0)
	{
		  if(close(fd) == -1)
        {
            perror("close");
            return UART_ERR_CLOSE;
        }
		return UART_ERR_MUTEX;
	}
				
	// 初始化cond
	if(pthread_cond_init(&ctx->rx_cond, NULL) != 0)
	{
        pthread_mutex_destroy(&ctx->rx_mutex);
		  if(close(fd) == -1)
        {
            perror("close");
            return UART_ERR_CLOSE;
        }
		return UART_ERR_COND;
	}
				
	ctx->fd = fd;
	return UART_OK;
				
}

UartResult uart_destroy(UartContext *ctx)
{
    //检查参数
    if(ctx == NULL)
    {
    	return UART_ERR_PARAM;
    }

    // 销毁条件变量
    if(pthread_cond_destroy(&ctx->rx_cond) != 0)
    {
        return UART_ERR_COND;
    }

    // 销毁互斥锁
    if(pthread_mutex_destroy(&ctx->rx_mutex) != 0)
    {
        return UART_ERR_MUTEX;
    }
    // fd置为-1 并且关闭串口
        if (ctx->fd != -1)
    {
        int ret = close(ctx->fd);

        ctx->fd = -1;

        if (ret == -1)
        {
            perror("close");
            return UART_ERR_CLOSE;
        }
    }

    return UART_OK;   
}
void *uart_rx_thread(void *arg)
{
    UartContext *ctx = (UartContext*)arg;
    unsigned char buf[RX_SIZE];

    ssize_t n = read(ctx->fd, buf, sizeof(buf));
    
    return NULL;
}