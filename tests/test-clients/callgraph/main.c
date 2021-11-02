#include <stdio.h>

__attribute__((noinline)) int idle()
{
    int k = 0;
    for (int i = 0; i < 2000000; i++) {
        k += i;
    }
    return k;
}

__attribute__((noinline)) int child3()
{
    return idle();
}

__attribute__((noinline)) int child2()
{
    return child3() + idle();
}

__attribute__((noinline)) int child1()
{
    return child2() + idle();
}

__attribute__((noinline)) int test()
{
    return child1() + idle();
}

__attribute__((noinline)) int parent3()
{
    return idle() + test();
}

__attribute__((noinline)) int parent2()
{
    return parent3() + idle();
}

__attribute__((noinline)) int parent1()
{
    return parent2() + idle();
}

int main()
{
    idle();
    printf("%i\n", parent1());
    return 0;
}
