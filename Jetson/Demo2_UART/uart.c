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
    int fd = open("/dev/ttyTHS1",O_RDWR|O_NOCTTY);
    
    if(fd == -1)
    {
		perror("fd打开失败");
        return UART_ERR_OPEN;
    }
    
    // 配置串口
    int uart_configure = uart_configure();
    if(uart_configure == -1)
    {
		perror("函数调用失败");
        close(fd);
        return UART_ERR_CONFIG;
    }