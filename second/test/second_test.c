#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

void main(void)
{
    int fd;
    int counter = 0;
    int old_counter = 0;

    fd = open("/dev/second", O_RDONLY);
    if (-1 != fd) {
        while (1) {
            read(fd, &counter, sizeof(unsigned int));
            if (counter != old_counter) {
                printf("seconds after open /dev/second: %d\n", counter);
                old_counter = counter;
            }
        }
    } else {
        printf("Device open failure.\n");
    }
}
