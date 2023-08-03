#include <stdio.h>

__attribute__((noinline)) int idle(void)
{
    int k = 0;
    for (int i = 0; i < 2000000; i++) {
        k += i;
    }
    return k;
}

__attribute__((noinline)) int child3(void)
{
    return idle();
}

__attribute__((noinline)) int child2(void)
{
    return child3() + idle();
}

__attribute__((noinline)) int child1(void)
{
    return child2() + idle();
}

__attribute__((noinline)) int test(void)
{
    return child1() + idle();
}

__attribute__((noinline)) int parent3(void)
{
    return idle() + test();
}

__attribute__((noinline)) int parent2(void)
{
    return parent3() + idle();
}

__attribute__((noinline)) int parent1(void)
{
    return parent2() + idle();
}

int main(void)
{
    idle();
    printf("%i\n", parent1());
    return 0;
}
