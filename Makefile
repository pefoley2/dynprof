ifeq ($(origin CXX),default)
CXX = clang++
endif

CXXFLAGS := $(strip $(CXXFLAGS) -fno-exceptions -fvisibility=hidden -march=native -pipe)
CXXFLAGS += -Wall -Wextra -Winvalid-pch -Wpedantic -Weffc++ -D_GLIBCXX_USE_CXX11_ABI=0
CXXFLAGS += -std=c++11

ifneq ($(filter clang++,$(CXX)),)
CXXFLAGS += -Weverything -Wno-c++98-compat
endif

CXXFLAGS += -ggdb3
#CXXFLAGS += -O2
#CXXFLAGS += -flto
#CXXFLAGS += -floop-interchange -floop-strip-mine -floop-block -fgraphite-identity
#CXXFLAGS += -fsanitize=address
#CXXFLAGS += -fsanitize=thread
#CXXFLAGS += -fsanitize=memory
#CXXFLAGS += -fsanitize=undefined

MAKEFLAGS=rR

LDFLAGS := $(strip $(LDFLAGS) -Wl,-O1 -Wl,--as-needed)

all: dynprof libdynprof.so example/test example/time

example/%: example/%.cc
	$(CXX) $(filter-out -fsanitize=%,$(CXXFLAGS)) -o $@ $<

%.h.gch: %.h
	$(CXX) -x c++-header $(CXXFLAGS) -o $@ $<

dynprof.h: dyninst.h.gch

%.so: %.cc %.h
	$(CXX) $(filter-out -fsanitize=%,$(CXXFLAGS)) $(LDFLAGS) -shared -fPIC -o $@ $<

%.o: %.cc dynprof.h
	$(CXX) $(CXXFLAGS) -include dyninst.h -c $< -o $@

dynprof: dynprof.o main.o
	$(CXX) $(CXXFLAGS) -ldyninstAPI -lsymtabAPI $(LDFLAGS) -o $@ $^

format:
	clang-format -i -style="{BasedOnStyle: google, IndentWidth: 4, ColumnLimit: 100}" *.cc *.h example/*.cc

analyze:
	@for x in libdynprof.cc main.cc dynprof.cc; do \
	echo $$x; \
	clang++ --analyze $(CXXFLAGS) -o /dev/null $$x; \
	done

tidy:
	test -d work || (mkdir work && cd work && cmake ..)
	make -C work
	clang-tidy -analyze-temporary-dtors -header-filter='.*' -checks='*,-llvm-header-guard' -p work *.cc

test: all
	./dynprof example/test

binary: test_dynprof

test_dynprof: dynprof libdynprof.so example/test
	./dynprof --write example/test

run: test_dynprof
	./test_dynprof

clean:
	rm -rf *.o *.so dyninst.h.gch dynprof test_dynprof example/test example/time work

.PHONY: all format analyze tidy test binary run clean
