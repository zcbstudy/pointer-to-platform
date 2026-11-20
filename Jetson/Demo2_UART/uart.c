#include "uart.h"

#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

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

    if(pthread_mutex_init(&ctx->tx_mutex,NULL) != 0)
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
        pthread_mutex_destroy(&ctx->tx_mutex);
		if(close(fd) == -1)
        {
            perror("close");
            return UART_ERR_CLOSE;
        }
		return UART_ERR_COND;
	}
				
    if(pthread_cond_init(&ctx->tx_cond, NULL) != 0)
    {
        pthread_mutex_destroy(&ctx->rx_cond);
        pthread_mutex_destroy(&ctx->tx_mutex);
        pthread_mutex_destory(&ctx->rx_mutex);
        if(close(fd) == 1)
        {
            perror("close");
            return UART_ERR_CLOSE;
        }
        return UART_ERR_CLOSE;
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

    //加一个pollfd结构体
    //为什么	tty.c_cc[VMIN] = 1;
	//         tty.c_cc[VTIME] = 0;
    //我们原来川口的配置是这个 但是这意味着没有数据情况下 read()会一直阻塞
    // 即使主程序将ctx->stop设置成为1 接受程序也不一定能够即使推出
    struct pollfd pfd;
    // fd 希望监听哪一个文件描述符
    pfd.fd = ctx->fd;
    // events 希望监听到什么事件
    // POLLIN表示有数据可读
    pfd.events = POLLIN;
    // revents表示实际发生了什么事件
    pfd.revents = 0;
    while(1)
    {
        pthread_mutex_lock(&ctx->rx_mutex);
        int stop = ctx->stop;
        pthread_mutex_unlock(&ctx->rx_mutex);
        // 检查stop是否停止
        if(stop == 1)
        {
            break;
        }
        int ret = poll(&pfd,1,100);
        if(ret == 0)
        {
            // 超时
            continue;
        }
        if(ret == -1)
        {
            if(errno == EINTR)
            {
                continue;
            }
            perror("poll");
            break;
        }
        //POLLIN可读 0x01 
        //POLLOUT可写 0x04
        //POLLERR错误 0x08
        //POLLHUP挂断 0x10
        //POLLNVAL文件描述符不是一个打开的文件 0x20
        //假设串口有数据 同时发生了错误
        //那么pfd.rvents = POLLIN|POLLERR;
        //对应二进制 0000 1001 但是偶们想知道有没有POLLIN事件 就用按位与
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
        {
            fprintf(stderr, "UART poll error: 0x%x\n",(unsigned int)pfd.revents);

             break;
        }
        if(pfd.revents & POLLIN)
        {
            ssize_t n = read(ctx->fd, buf, sizeof(buf));
            if(n > 0)
            {
                 // 枷锁
                pthread_mutex_lock(&ctx->rx_mutex);
                if((size_t)n <= RX_SIZE - ctx->rx_len)
                {
                    // copy数据
                    memcpy(&ctx->rx_buf[ctx->rx_len],buf,(size_t)n);
                    // 更新长度
                    ctx->rx_len += (size_t)n;
                }else
                {
                    pthread_mutex_unlock(&ctx->rx_mutex);
                    fprintf(stderr,"RX buffer overflow\n");
                    break;
                }
                 // 唤醒条件变量
                pthread_cond_signal(&ctx->rx_cond);
                // 解锁
                pthread_mutex_unlock(&ctx->rx_mutex);
            }else if(n == 0)
            {
                // 读到0 表示对端关闭
                break;   
            }else if(n == -1)
            {
                //EINTR 表示 Interrupted system call（系统调用被中断）。
                if(errno == EINTR)
                {
                    continue;
                }
                perror("read");
                break;
            } 
        }
    }
    pthread_mutex_lock(&ctx->rx_mutex);
    ctx->stop = 1;
    pthread_cond_broadcast(&ctx->rx_cond);
    pthread_mutex_unlock(&ctx->rx_mutex);
    return NULL;
}

void *uart_tx_thread(void *arg)
{
    UartContext *ctx = (UartContext *)arg;
    unsigned char buf[TX_SIZE];
    struct pollfd pfd;
    // 键盘标准输入也是一个文件描述符
    // STDIN_FILENO
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    pfd.revents = 0;

    while(1)
    {
        pthread_mutex_lock(&ctx->rx_mutex);
        int stop = ctx->stop;
        pthread_mutex_unlock(&ctx->rx_mutex);
        // 检查stop是否停止
        if(stop == 1)
        {
            break;
        }
        int ret = poll(&pfd,1,100);
        if(ret == 0)
        {
            // 超时
            continue;
        }
        if(ret == -1)
        {
            if(errno == EINTR)
            {
                continue;
            }
            perror("poll");
            break;
        }
        //POLLIN可读 0x01 
        //POLLOUT可写 0x04
        //POLLERR错误 0x08
        //POLLHUP挂断 0x10
        //POLLNVAL文件描述符不是一个打开的文件 0x20
        //假设串口有数据 同时发生了错误
        //那么pfd.rvents = POLLIN|POLLERR;
        //对应二进制 0000 1001 但是偶们想知道有没有POLLIN事件 就用按位与
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
        {
            fprintf(stderr, "UART poll error: 0x%x\n",(unsigned int)pfd.revents);

             break;
        }
        if(pfd.revents &POLLIN)
        {
            // 把键盘的数据当作文件读进
            ssize_t n = read(STDIN_FILENO,buf,sizeof(buf));
            int count = 0;
            if(n > 0)
            {
                ssize_t w = write(ctx->fd,buf,(size_t)n);
                if(w > 0)
                {
                    pthread_mutex_lock(&ctx->tx_mutex);
                    memcpy(&ctx->tx_buf[ctx->tx_len],buf,(size_t)n);
                    ctx->tx_len += (size_t)n;
                }else{
                    pthread_mutex_unlock(&ctx->tx_mutex);
                    fprintf(stderr,"TX buffer overflow\n");
                    break;
                }
                // 唤醒条件变量
                pthread_cond_signal(&ctx->tx_cond);
                // 解锁
                pthread_mutex_unlock(&ctx->tx_mutex);
            }
        }
    }
}