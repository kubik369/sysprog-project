default: all
all:
	mkdir -p build
	gcc -Wall -std=c11 -o build/utalk src/utalk.c

clean:
	rm -rf build
