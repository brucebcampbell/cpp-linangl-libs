#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "testidwunit.h"

int main(int argc, char **argv)
{
    unsigned seed;
    if( argc==2 )
        seed = (unsigned)atoi(argv[1]);
    else
    {
        time_t t;
        seed = (unsigned)time(&t);
    }
    srand(seed);
    try
    {
        if(!testidwunit_test_silent())
            throw 0;
    }
    catch(...)
    {
        printf("%-32s FAILED(seed=%ld)\n", "idwint", (long)seed);
        return 1;
    }
    printf("%-32s OK\n", "idwint");
    return 0;
}

