#include <stdio.h>
#include <unistd.h>

void foo(double);
void bar(double);
void baz(double);

void foo(double x) {
    bar(x);
    sleep(x);
    baz(x);
}
void bar(double x) { sleep(x); }
void baz(double x) { sleep(x); }

int main() {
    foo(0.01);
    return 0;
}
