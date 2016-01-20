CXX = g++
#CXX = clang++ -stdlib=libc++

CFLAGS = -fno-exceptions
CFLAGS += -Wall -Wextra -Winvalid-pch -Wpedantic -Wno-c++98-compat
#CFLAGS += -Weverything
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
	make -C work
	clang-tidy -analyze-temporary-dtors -header-filter='.*' -checks='*,-llvm-header-guard' -p work dynprof.cc

cppcheck:
	cppcheck -I/usr/lib/gcc/x86_64-pc-linux-gnu/5.3.0/include/g++-v5 \
	-I/usr/lib/gcc/x86_64-pc-linux-gnu/5.3.0/include/g++-v5/x86_64-pc-linux-gnu \
	-I/usr/lib/gcc/x86_64-pc-linux-gnu/5.3.0/include/g++-v5/backward \
	-I/usr/lib/gcc/x86_64-pc-linux-gnu/5.3.0/include \
	-I/usr/local/include \
	-I/usr/lib/gcc/x86_64-pc-linux-gnu/5.3.0/include-fixed \
	-I/usr/include \
	--enable=all --max-configs=1 --platform=unix64 --inconclusive .


test: all
	./dynprof example/test

.PHONY: all format analyze tidy test
