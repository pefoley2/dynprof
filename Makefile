CC = gcc
CXX = g++
#CC = clang
#CXX = clang++

CFLAGS = -Wall -Wextra -Winvalid-pch -Wpedantic
CFLAGS += -ggdb3
#CFLAGS += -O2
#CFLAGS += -fsanitize=address

CXXFLAGS = $(CFLAGS) -std=c++11 -Weffc++
LDFLAGS = -L/usr/local/lib -ldyninstAPI

all: dynprof example/test

example/test: example/test.c
	$(CC) $(CFLAGS) -o $@ $<

%.h.gch: %.h
	$(CXX) -x c++-header $(CXXFLAGS) -o $@ $<

dynprof: dynprof.cc dynprof.h dyninst.h.gch
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -include dyninst.h -o $@ $<

format:
	clang-format -i -style="{BasedOnStyle: google, IndentWidth: 4, ColumnLimit: 100}" *.cc *.h example/*.c

analyze:
	clang++ --analyze --std=c++11 -o /dev/null dynprof.cc

test: all
	./dynprof example/test

.PHONY: all format analyze test
