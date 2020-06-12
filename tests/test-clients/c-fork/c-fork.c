#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <math.h>

int main()
{
    pid_t child = fork();
    if (child == 0) {
        double sum = 0;
        int i = 0;
        for (; i < 1000000; ++i) {
            sum += cos(cos(i) * cos(i) + cos(i * i) + cos(cos(i)));
        }
        printf("sum is: %g\n", sum);
    } else {
        printf("waiting for child\n");
        waitpid(child, NULL, 0);
        printf("done waiting\n");
    }
    return 0;
}
