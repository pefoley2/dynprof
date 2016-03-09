#include <stdio.h>  // for printf
#include <time.h>   // for CLOCK_MONOTONIC

int main() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    printf("%ld:%ld\n", t.tv_sec, t.tv_nsec);
    return 0;
}
