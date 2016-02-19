#CXX ?= g++
CXX = clang++

CFLAGS := $(strip $(CFLAGS) -fno-exceptions -std=c++11)
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


LDFLAGS := $(LDFLAGS) -L/usr/local/lib -ldyninstAPI

all: dynprof example/test

example/test: example/test.cc
	$(CXX) $(CFLAGS) -fno-sanitize=all -o $@ $<

%.h.gch: %.h
	$(CXX) -x c++-header $(CFLAGS) -o $@ $<

dynprof: dynprof.cc dynprof.h dyninst.h.gch
	$(CXX) $(CFLAGS) $(LDFLAGS) -include dyninst.h -o $@ $<

format:
	clang-format -i -style="{BasedOnStyle: google, IndentWidth: 4, ColumnLimit: 100}" *.cc *.h example/*.cc

analyze:
	clang++ --analyze $(CFLAGS) -o /dev/null dynprof.cc

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
binary: all
	./dynprof --write example/test

.PHONY: all format analyze tidy test
