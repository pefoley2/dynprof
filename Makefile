dynprof: dynprof.cc
	g++ -Wall -Wextra -ldyninstAPI -o $@ $<
