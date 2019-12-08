#include <stdio.h>
#include <stdlib.h>

int main(int argc, char const *argv[]) {
    int N = 512;

    printf("%d\n", N);
    for(int i = N - 1; i > N/2; i--){
        printf("fault 40.0 %d\n", i);
    }
    printf("propose 50.0 %d\n", N/2 - 1);
    return 0;
}
