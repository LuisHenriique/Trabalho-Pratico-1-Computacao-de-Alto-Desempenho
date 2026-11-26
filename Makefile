all:
	mpicc -O3 -fopenmp -Wall -o fire_mpi fire_mpi.c
run:
	mpirun fire_mpi entradas/entrada_carga_grande.txt