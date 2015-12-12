CFLAGS = -Wall -Wextra
CXXFLAGS = -std=c++11 -Wall -Wextra -L/usr/local/lib -ldyninstAPI
CFLAGS += -ggdb3
#CFLAGS += -O2
#CFLAGS += -fsanitize=address
all: dynprof example/test
example/test: example/test.c
	gcc $(CFLAGS) -o $@ $<
dynprof: dynprof.cc
	g++ $(CFLAGS) $(CXXFLAGS) -o $@ $<

test: all
	./dynprof example/test
