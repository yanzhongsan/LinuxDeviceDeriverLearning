#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>      /*open等函数的头文件*/
#include <signal.h>

static void signal_io_handler(int signum)
{
    printf("receive a signal from globalfifo, signalnum: %d\n", signum);
}

int main(void)
{
    int fd,count,device,oflags;
    char buf_write[] = "test globalfifo data!";
    char buf_read[4096];
    char path[20];

    for (device=0; device < 10; device++) {
        snprintf(path, 20, "/dev/globalfifo_%d", device);
        if (-1 == (fd = open(path, O_RDWR))) {
            printf("open device file %s error.\n", path);
            continue;
        } else {
            printf("Open device file %s success!\n", path);
            printf("Write \"%s\" to %s\n", buf_write, path);
            write(fd, buf_write, sizeof(buf_write));
            lseek(fd, 0, SEEK_SET);
            read(fd, buf_read, sizeof(buf_write));
            printf("Read  \"%s\" from %s\n", buf_read, path);
            printf("\n\n");

            close(fd);
        }
    }

    fd = open(path, O_RDWR, S_IRUSR | S_IWUSR);
    if (-1 != fd) {
        printf("device file %s open success.\n", path);
        signal(SIGIO, signal_io_handler);
        fcntl(fd, F_SETOWN, getpid());
        oflags = fcntl(fd, F_GETFL);
        fcntl(fd, F_SETFL, oflags | FASYNC);
        while (1) {
            sleep(100);
        }
    } else {
        printf("device file %s open failure.\n", path);
    }
    
    return 0;
}
