all:
	gcc -g -fopenmp -o fire_seq fire_seq.c
omp:
	gcc -g -fopenmp -o fire_omp fire_omp.c
dyn:
	gcc -g -fopenmp -o fire_omp_d fire_omp_dynamic.c
strict:
	gcc -g -fopenmp -Wall -Wextra -Wpedantic -Wshadow -Wconversion -o exec fire_seq.c 
