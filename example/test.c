#include <stdio.h>
#include <unistd.h>

void foo(unsigned int);
void bar(unsigned int);
void baz(unsigned int);

void foo(unsigned int x) {
    bar(x);
    sleep(x);
    baz(x);
}
void bar(unsigned int x) { sleep(x); }
void baz(unsigned int x) { sleep(x); }

int main() {
    foo(1);
    return 0;
}
