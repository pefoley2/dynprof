CFLAGS = -std=c++11 -Wall -Wextra -ldyninstAPI
CFLAGS += -ggdb3
#CFLAGS += -fsanitize=address
dynprof: dynprof.cc
	clang++ $(CFLAGS) -o $@ $<
