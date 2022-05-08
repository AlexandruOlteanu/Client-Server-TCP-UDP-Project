#<Copyright Alexandru Olteanu, grupa 322CA, alexandruolteanu2001@gmail.com>
CC = g++
#CFLAGS = -Wall -Wextra -O2
LDLIBS = -lm

build: 
	g++ server.cpp -o server 
	g++ subscriber.cpp -o subscriber 

.PHONY: clean

clean:
	rm -rf *.o server subscriber