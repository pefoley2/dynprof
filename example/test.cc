#include <unistd.h>  // for usleep

void foo(unsigned int);
void bar(unsigned int);
void baz(unsigned int);

void foo(unsigned int x) {
    bar(x);
    usleep(x);
    baz(x);
    baz(x);
}
void bar(unsigned int x) { usleep(x); }
void baz(unsigned int x) { usleep(x); }

int main() {
    foo(200000);
    return 0;
}
