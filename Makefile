# FIXME: g++ apparently mis-compiles dynprof.
ifeq ($(origin CXX),default)
CXX = clang++
endif

CXXFLAGS := $(strip $(CXXFLAGS) -fno-exceptions -fvisibility=hidden -march=native -pipe -fPIC)
CXXFLAGS += -Wall -Wextra -Winvalid-pch -Wpedantic -Weffc++ -D_GLIBCXX_USE_CXX11_ABI=1
CXXFLAGS += -std=c++14

ifneq ($(filter clang++,$(CXX)),)
CXXFLAGS += -Weverything -Wno-c++98-compat
endif

#CXXFLAGS += -ggdb3
#CXXFLAGS += -O2
#CXXFLAGS += -flto
#CXXFLAGS += -floop-interchange -floop-strip-mine -floop-block -fgraphite-identity
#CXXFLAGS += -fsanitize=address
#CXXFLAGS += -fsanitize=thread
#CXXFLAGS += -fsanitize=memory
#CXXFLAGS += -fsanitize=undefined

MAKEFLAGS=rR

LDFLAGS := $(strip $(LDFLAGS) -Wl,-O1 -Wl,--as-needed -Wl,--no-undefined)

all: dynprof display libdynprof.so example/test example/time

example/%: example/%.cc
	$(CXX) $(filter-out -fsanitize=%,$(CXXFLAGS)) -o $@ $<

%.h.gch: %.h
	$(CXX) -x c++-header $(CXXFLAGS) -o $@ $<

dynprof.h: dyninst.h.gch

%.so: %.cc %.h
	$(CXX) $(filter-out -fsanitize=%,$(CXXFLAGS)) -lstackwalk $(LDFLAGS) -shared -o $@ $<

display: display.cc display.h
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $<


%.o: %.cc dynprof.h
	$(CXX) $(CXXFLAGS) -include dyninst.h -c $< -o $@

dynprof: dynprof.o main.o
	$(CXX) $(CXXFLAGS) -lcommon -lstackwalk -ldyninstAPI -lsymtabAPI $(LDFLAGS) -o $@ $^

format:
	clang-format -i -style="{BasedOnStyle: google, IndentWidth: 4, ColumnLimit: 100}" *.cc *.h example/*.cc

analyze:
	@for x in libdynprof.cc main.cc dynprof.cc display.cc; do \
	echo $$x; \
	clang++ --analyze $(CXXFLAGS) -o /dev/null $$x; \
	done

tidy:
	test -d work || (mkdir work && cd work && cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ ..)
	make -C work
	clang-tidy -analyze-temporary-dtors -header-filter='.*' -checks='*,-llvm-header-guard,-llvm-include-order,-readability-implicit-bool-cast,-cppcoreguidelines-pro-bounds-array-to-pointer-decay,-cppcoreguidelines-pro-type-vararg,-cppcoreguidelines-pro-type-const-cast,-cppcoreguidelines-pro-bounds-pointer-arithmetic' -p work *.cc

test: all
	./dynprof example/test

binary: test_dynprof

test_dynprof: dynprof libdynprof.so example/test
	./dynprof --write example/test

run: test_dynprof
	./test_dynprof

output: display
	./display out_dynprof.*

clean:
	rm -rf *.o *.so out_dynprof.* dyninst.h.gch display dynprof test_dynprof example/test example/time work

.PHONY: all format analyze tidy test binary run clean output
