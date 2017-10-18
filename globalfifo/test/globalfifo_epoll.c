#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>      /*open等函数的头文件*/
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>

#define FIFO_CLEAR  0x01
#define BUFFER_LEN  20

void main(void)
{
    int fd;

    fd = open("/dev/globalfifo_0", O_RDONLY | O_NONBLOCK);
    if (-1 != fd) {
        struct epoll_event ev_globalfifo;
        int err;
        int epfd;

        if (ioctl(fd, FIFO_CLEAR, 0) < 0) {
            printf("ioctl command failed\n");
        }
        epfd = epoll_create(1);
        if (epfd < 0) {
            perror("epoll_create()");
            return;
        }

        bzero(&ev_globalfifo, sizeof(struct epoll_event));
        ev_globalfifo.events = EPOLLIN | EPOLLPRI;
        err = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev_globalfifo);
        if (err < 0) {
            perror("epoll_ctl()");
            return;
        }
        err = epoll_wait(epfd, &ev_globalfifo, 1, 15000);
        if (err < 0) {
            perror("epoll_wait()");
        } else if (err == 0) {
            printf("No data input in FIFO within 15 seconds.\n");
        } else {
            printf("FIFO is not empty.\n");
        }
        err = epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &ev_globalfifo);
        if (err < 0) {
            perror("epoll_ctl()");
        }
    } else {
        printf("Device open failure.\n");
    }
}
