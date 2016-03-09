ifeq ($(origin CXX),default)
#CXX = g++
CXX = clang++
endif

CFLAGS := $(strip $(CFLAGS) -fno-exceptions -fvisibility=hidden -march=native -pipe)
CFLAGS += -Wall -Wextra -Winvalid-pch -Wpedantic
ifneq ($(filter clang++,$(CXX)),)
CFLAGS += -Weverything -Wno-c++98-compat
endif
CFLAGS += -ggdb3
#CFLAGS += -O2
#CFLAGS += -flto
#CFLAGS += -floop-interchange -floop-strip-mine -floop-block -fgraphite-identity
#CFLAGS += -fsanitize=address
#CFLAGS += -fsanitize=thread
#CFLAGS += -fsanitize=memory
#CFLAGS += -fsanitize=undefined

CXXFLAGS = $(CFLAGS) -std=c++11 -Weffc++

MAKEFLAGS=rR

LDFLAGS := $(strip $(LDFLAGS) -Wl,-O1 -Wl,--as-needed)

all: dynprof libdynprof.so example/test example/time

example/%: example/%.cc
	$(CXX) $(filter-out -fsanitize=%,$(CXXFLAGS)) -o $@ $<

%.h.gch: %.h
	$(CXX) -x c++-header $(CXXFLAGS) -o $@ $<

dynprof.h: dyninst.h.gch

libdynprof.so: libdynprof.c
	$(CXX) $(filter-out -fsanitize=%,$(CFLAGS)) $(LDFLAGS) -xc -std=c11 -shared -fPIC -o $@ $^

%.o: %.cc dynprof.h
	$(CXX) $(CXXFLAGS) -include dyninst.h -c $< -o $@

dynprof: dynprof.o main.o
	$(CXX) $(CXXFLAGS) -ldyninstAPI -lsymtabAPI $(LDFLAGS) -o $@ $^

format:
	clang-format -i -style="{BasedOnStyle: google, IndentWidth: 4, ColumnLimit: 100}" *.c *.cc *.h example/*.cc

analyze:
	clang++ --analyze -xc $(CFLAGS) -o /dev/null libdynprof.c
	clang++ --analyze $(CXXFLAGS) -o /dev/null main.cc
	clang++ --analyze $(CXXFLAGS) -o /dev/null dynprof.cc

tidy:
	test -d work || (mkdir work && cd work && cmake ..)
	make -C work
	clang-tidy -analyze-temporary-dtors -header-filter='.*' -checks='*,-llvm-header-guard' -p work *.cc

test: all
	./dynprof example/test

binary: test_dynprof

test_dynprof: dynprof example/test
	./dynprof --write example/test

run: test_dynprof
	./test_dynprof

clean:
	rm -rf *.o *.so dyninst.h.gch dynprof test_dynprof example/test example/time work

.PHONY: all format analyze tidy test binary run clean
