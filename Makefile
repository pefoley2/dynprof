CXX = g++
#CXX = clang++

CFLAGS = -Wall -Wextra -Winvalid-pch -Wpedantic
#CFLAGS += -Weverything -Wno-c++98-compat
CFLAGS += -ggdb3
#CFLAGS += -O2
#CFLAGS += -flto -fuse-linker-plugin
#CFLAGS += -floop-interchange -floop-strip-mine -floop-block -fgraphite-identity
#CFLAGS += -fsanitize=address
#CFLAGS += -fsanitize=thread


CXXFLAGS = $(CFLAGS) -std=c++11 -Weffc++
LDFLAGS = -L/usr/local/lib -ldyninstAPI

all: dynprof example/test

example/test: example/test.c
	$(CXX) $(CFLAGS) -o $@ $<

%.h.gch: %.h
	$(CXX) -x c++-header $(CXXFLAGS) -o $@ $<

dynprof: dynprof.cc dynprof.h dyninst.h.gch
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -include dyninst.h -o $@ $<

format:
	clang-format -i -style="{BasedOnStyle: google, IndentWidth: 4, ColumnLimit: 100}" *.cc *.h example/*.c

analyze:
	clang++ --analyze --std=c++11 -o /dev/null dynprof.cc

tidy:
	clang-tidy -checks='*' -p work dynprof.cc

test: all
	./dynprof example/test

.PHONY: all format analyze tidy test
