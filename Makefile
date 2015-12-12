#CFLAGS = -fsanitize=address
dynprof: dynprof.cc
	clang++ $(CFLAGS) -ggdb3 -std=c++11 -Wall -Wextra -ldyninstAPI -o $@ $<
