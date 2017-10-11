#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>      /*open等函数的头文件*/

int main(void)
{
    int fd,count,device;
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

    return 0;
}
