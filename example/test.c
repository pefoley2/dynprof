#include <stdio.h>
#include <unistd.h>

void foo(int x) {
    sleep(x);
}

int main() {
    //printf("Before\n");
    foo(1);
    //printf("After\n");
    return 0;
}
