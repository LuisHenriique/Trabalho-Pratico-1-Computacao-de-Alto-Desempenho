all:
	mpicc -O3 -fopenmp -Wall -o fire_mpi_omp fire_mpi_omp.c
	gcc -O3 -fopenmp -Wall -o fire_seq fire_seq.c
run:
	mpirun fire_mpi_omp entradas/entrada_carga_grande.txt

comp:
	@mkdir -p resultados
	@set -e; \
	for carga in pequena media grande; do \
		./fire_seq entradas/entrada_carga_$$carga.txt > resultados/seq_$$carga.txt; \
		\
		case $$carga in \
			pequena) nome=Pequeno;; \
			media) nome=Médio;; \
			grande) nome=Grande;; \
		esac; \
		\
		seq_tempo=$$(awk '/^tempo:/ {print $$2}' resultados/seq_$$carga.txt); \
		echo "$$nome: Seq - $$seq_tempo s"; \
		\
		for core in 1 2 4 8; do \
			mpirun -np $$core ./fire_mpi_omp entradas/entrada_carga_$$carga.txt > resultados/mpi_$$carga.txt; \
			mpi_tempo=$$(awk '/^tempo:/ {print $$2}' resultados/mpi_$$carga.txt); \
		\
			if diff -I '^tempo:' resultados/seq_$$carga.txt resultados/mpi_$$carga.txt > /dev/null; then \
				resultado=Iguais; \
			else \
				resultado=Diferentes; \
			fi; \
		\
			echo "Cores - $$core | MPI - $$mpi_tempo s | Sp - $$(echo "scale=2; $$seq_tempo / $$mpi_tempo" | bc) | $$resultado"; \
		done; \
	done
