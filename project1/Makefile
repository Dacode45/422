CC=gcc
CFLAGS=-I.
DEPS = record.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

generate: generate.c dump.c sort_thresh.c
	$(CC) -o generate generate.c -Wall -Werror -I.
	$(CC) -o dump dump.c -Wall -Werror -I.
	$(CC) -o sort_thresh sort_thresh.c -Wall  -I.

test: generate
	./test.sh
