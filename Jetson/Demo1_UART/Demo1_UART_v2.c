#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
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
int main()
{
	int fd = open("/dev/ttyTHS1",O_RDWR | O_NOCTTY);
	
	if(fd == -1)
	{
		perror("open");
		return 1;
	}
	if(uart_configure(fd) == -1)
	{
		close(fd);
		return 1;
	}
    
    printf("Jetson UART configured successfully.\n");

    //当前3566发送字符为hello 5字节
    char buf[6];
    size_t received = 0;

    while(received < 5)
    {
        ssize_t n = read(fd, buf + received, 5 - received);

        if(n == -1)
        {
            if(errno == EINTR)
                continue;
            perror("read");
            close(fd);
            return 1;
        }
        
        if(n == 0)
        {
            printf("No data received.\n");
            close(fd);
            return 1;
        }
        received += (size_t)n;
    }
    buf[5] = '\0';
    printf("Received: %s\n", buf);
    close(fd);
    return 0;
}