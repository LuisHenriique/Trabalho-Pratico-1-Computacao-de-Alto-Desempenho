all:
	gcc -g -fopenmp -o fire_seq fire_seq.c
parallel:
	gcc -g -fopenmp -o fire_omp fire_omp.c
strict:
	gcc -g -fopenmp -Wall -Wextra -Wpedantic -Wshadow -Wconversion -o exec fire_seq.c 
