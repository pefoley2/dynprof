#CC = gcc
#CXX = g++
CC = clang
CXX = clang++

CFLAGS = -Wall -Wextra -Winvalid-pch
CFLAGS += -ggdb3
#CFLAGS += -O2
CFLAGS += -fsanitize=address

CXXFLAGS = $(CFLAGS) -std=c++11
LDFLAGS = -L/usr/local/lib -lcommon -ldyninstAPI

all: dynprof example/test

example/test: example/test.c
	$(CC) $(CFLAGS) -o $@ $<

%.h.gch: %.h
	$(CXX) -x c++-header $(CXXFLAGS) -o $@ $<

dynprof: dynprof.cc dyninst.h.gch
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -include dyninst.h -o $@ $<

test: all
	./dynprof example/test
