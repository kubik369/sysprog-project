default: all
all:
	gcc -Wall -std=c11 -O2 -o utalk src/utalk.c -lncurses

clean:
	rm utalk
