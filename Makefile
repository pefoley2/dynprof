CFLAGS = -Wall -Wextra -Winvalid-pch
CFLAGS += -ggdb3
#CFLAGS += -O2
CFLAGS += -fsanitize=address
CXXFLAGS = $(CFLAGS) -std=c++11
LDFLAGS = -L/usr/local/lib -lcommon -ldyninstAPI
all: dynprof example/test
example/test: example/test.c
	gcc $(CFLAGS) -o $@ $<
%.h.gch: %.h
	g++ $(CXXFLAGS) -o $@ $<
dynprof: dynprof.cc dyninst.h.gch
	g++ $(CXXFLAGS) $(LDFLAGS) -o $@ $<

test: all
	./dynprof example/test
