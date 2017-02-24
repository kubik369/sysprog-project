default: all
all:
	gcc -Wall -std=c11 -o utalk utalk.c

clean:
	rm -f utalk
