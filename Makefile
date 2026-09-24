CC       := gcc
CFLAGS   := -g -fopenmp

.PHONY: all clean

all: fire_seq fire_omp fire_omp_d

fire_seq: fire_seq.c
	$(CC) $(CFLAGS) -o $@ $<

fire_omp: fire_omp.c
	$(CC) $(CFLAGS) -o $@ $<

fire_omp_d: fire_omp_dynamic.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f fire_seq fire_omp fire_omp_d
