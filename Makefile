CFLAGS = -std=c++11 -Wall -Wextra -ldyninstAPI
CFLAGS += -ggdb3
#CFLAGS += -fsanitize=address
all: dynprof example/test
example/test: example/test.c
	clang -Wall -Wextra -o $@ $<
dynprof: dynprof.cc
	clang++ $(CFLAGS) -o $@ $<

test: all
	./dynprof example/test
