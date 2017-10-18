#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>      /*open等函数的头文件*/
#include <signal.h>
#include <sys/ioctl.h>

#define FIFO_CLEAR  0x01
#define BUFFER_LEN  20

void main(void)
{
    int fd, num;
    char rd_ch[BUFFER_LEN];
    fd_set rfds, wfds;

    /*以非阻塞方式打开/dev/globalfifo设备文件*/
    fd = open("/dev/globalfifo_0", O_RDONLY | O_NONBLOCK);
    if (-1 != fd) {
        /*清空FIFO*/
        if (ioctl(fd, FIFO_CLEAR, 0) < 0) {
            printf("ioctl command failed\n");
        }

        while (1) {
            FD_ZERO(&rfds);
            FD_ZERO(&wfds);
            FD_SET(fd, &rfds);
            FD_SET(fd, &wfds);

            select(fd+1, &rfds, &wfds, NULL, NULL);
            if (FD_ISSET(fd, &rfds)) {
                printf("Poll monitor: can be read.\n");
            }
            if (FD_ISSET(fd, &wfds)) {
                printf("Poll monitor: can be written.\n");
            }
        }
    } else {
        printf("Device open failure.\n");
    }
}
