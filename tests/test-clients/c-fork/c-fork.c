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
        for (int i = 0; i < 1000000; ++i) {
            sum += atanh(cos(i) * cos(i) + sin(i * i) + erf(log(i)));
        }
        printf("sum is: %g\n", sum);
    } else {
        printf("waiting for child\n");
        waitpid(child, NULL, 0);
        printf("done waiting\n");
    }
    return 0;
}
