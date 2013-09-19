CC      ?= gcc
RM      ?= rm
CFLAGS  ?=-Wall -Werror  -g
all: clean test

test: omt.o test.o
	$(CC) omt.o test.o $(LDFLAGS) -o test
clean:
	$(RM) -f test *.o
