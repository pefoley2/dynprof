ifeq ($(origin CXX),default)
#CXX = g++
CXX = clang++
endif

CFLAGS := $(strip $(CFLAGS) -fno-exceptions -std=c++11 -march=native -pipe)
CFLAGS += -Wall -Wextra -Winvalid-pch -Wpedantic -Weffc++
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

MAKEFLAGS=rR

LDFLAGS := $(strip $(LDFLAGS) -ldyninstAPI -Wl,-O1 -Wl,--as-needed)

all: dynprof example/test example/time

example/%: example/%.cc
	$(CXX) $(filter-out -fsanitize=%,$(CFLAGS)) -o $@ $<

%.h.gch: %.h
	$(CXX) -x c++-header $(CFLAGS) -o $@ $<

dynprof.h: dyninst.h.gch

%.o: %.cc dynprof.h
	$(CXX) $(CFLAGS) -include dyninst.h -c $< -o $@

dynprof: dynprof.o main.o
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $^

format:
	clang-format -i -style="{BasedOnStyle: google, IndentWidth: 4, ColumnLimit: 100}" *.cc *.h example/*.cc

analyze:
	clang++ --analyze $(CFLAGS) -o /dev/null main.cc
	clang++ --analyze $(CFLAGS) -o /dev/null dynprof.cc

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
	rm -rf *.o dyninst.h.gch dynprof test_dynprof example/test example/time work

.PHONY: all format analyze tidy test binary run clean
