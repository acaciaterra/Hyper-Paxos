#include <stdio.h>
#include <stdlib.h>

int main(int argc, char const *argv[]) {
    int N = 0;
    N = atoi(argv[1]);
    printf("%d\n", N);
    for(int i = N - 1; i > 0; i = i-2){
        if(i != N/2 - 1)
            printf("fault 40.0 %d\n", i);
    }
    printf("propose 50.0 %d\n", N/2 - 1);
    return 0;
}
