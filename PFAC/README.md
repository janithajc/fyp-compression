## PFAC library integration into LZSS algorithms string matching

#### In order to use this please follow these steps

1. Download and make [PFAC library](https://github.com/pfac-lib/pfac "PFAC git repo")
2. Compile and link using the following terminal commands


**Compile -**
g++ -m64 -fopenmp -c -O2 -D_REENTRANT -Wall -I/usr/local/cuda-7.5/include -I<PFAC_HOME>/include -o lzssx.o lzssx.c

**Link -**
g++ -m64 -fopenmp -Wall -o lzssx.exe lzssx.o -L<PFAC_HOME>/lib -lpfac -L/usr/local/cuda-7.5/lib64 -lcudart -ldl -lpthread