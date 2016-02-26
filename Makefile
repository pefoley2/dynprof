ifeq ($(origin CXX),default)
#CXX = g++
CXX = clang++
endif

CFLAGS := -fno-exceptions -std=c++11 -march=native -pipe
CFLAGS += -Wall -Wextra -Winvalid-pch -Wpedantic -Weffc++
ifneq ($(filter clang++,$(CXX)),)
CFLAGS += -Weverything -Wno-c++98-compat
endif
CFLAGS += -ggdb3
#CFLAGS += -O2
#CFLAGS += -flto -fuse-linker-plugin
#CFLAGS += -floop-interchange -floop-strip-mine -floop-block -fgraphite-identity
#CFLAGS += -fsanitize=address
#CFLAGS += -fsanitize=thread
#CFLAGS += -fsanitize=memory
#CFLAGS += -fsanitize=undefined

MAKEFLAGS=rR


LDFLAGS := $(LDFLAGS) -L/usr/local/lib -ldyninstAPI -Wl,-O1 -Wl,--as-needed

all: dynprof example/test

example/test: example/test.cc
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
	clang++ --analyze $(CFLAGS) -o /dev/null *.cc

tidy:
	mkdir work
	(cd work && cmake ..)
	make -C work
	clang-tidy -analyze-temporary-dtors -header-filter='.*' -checks='*,-llvm-header-guard' -p work *.cc

test: all
	./dynprof example/test
binary: all
	./dynprof --write example/test

clean:
	rm -rf *.o dyninst.h.gch dynprof test_dynprof example/test work

.PHONY: all format analyze tidy test binary clean
