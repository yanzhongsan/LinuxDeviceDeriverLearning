all: app.o globalfifo_poll.o globalfifo_epoll.o
	cc -o globalfifo_test app.o
	cc -o globalfifo_poll globalfifo_poll.o
	cc -o globalfifo_epoll globalfifo_epoll.o

#globalfifo_test: app.o

#	cc -o globalfifo_test app.o

#globalfifo_poll: globalfifo_poll.o

#	cc -o globalfifo_poll globalfifo_poll.o

globalfifo_epoll.o: globalfifo_epoll.c
	cc -c globalfifo_epoll.c

globalfifo_poll.o: globalfifo_poll.c
	cc -c globalfifo_poll.c

app.o: app.c
	cc -c app.c

clean:
	rm *.o globalfifo_test globalfifo_poll
