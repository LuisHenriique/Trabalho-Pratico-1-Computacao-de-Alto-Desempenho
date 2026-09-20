#include "simulation.h"
#include "terrain.h"
#include "structs.h"
#include <stdlib.h>
#include <string.h> 
#include <assert.h>
#include <stdio.h>
#include <time.h>

int max(int a, int b)
{
	if (a > b) return a;
	return b;
}

void startFire(Terrain *t, FirePosArray *fires)
{
	for (long i = 0; i < fires->count; i++) { 
		size_t index = getCellIndex(t->columns, fires->data[i].row, fires->data[i].col);
		t->cells[index].state = ON_FIRE;
		t->cells[index].burningTime = initialBurnTime[t->cells[index].cover];
	}
}

int *createContainmentMap(ContainmentZoneArray *containments, size_t rows, size_t columns)
{
	size_t nCells = rows * columns;
	int *containmentMap = (int *) malloc(sizeof(int)*nCells);
	assert(containmentMap);
	memset(containmentMap, -1, sizeof(int)*nCells);

	for (int i = 0; i < containments->count; i++) {
		ContainmentZone zone = containments->data[i];

		for (int j = zone.rowX; j <= zone.rowY; j++) {
			for (int k = zone.colX; k <= zone.colY; k++) {
				size_t index = getCellIndex(columns, j, k);
				if(containmentMap[index] == -1 || containmentMap[index] > zone.step)
					containmentMap[index] = zone.step;
			}
		}
	}

	return containmentMap;
}

int ignitionPotential(Terrain *t, int index, Wind w)
{
	char directions[8][2] = {{-1,-1}, {-1,0}, {-1,1},
							 {0, -1} 		 ,{0, 1},
							 {1, -1}, {1, 0}, {1, 1}};

	int row = index/(int)t->columns;
	int col = index - row*(int)t->columns;

	int totalWeight = 0;

	for (int i = 0; i < 8; i++) {
		int neighborRow = row + directions[i][0];
		int neighborCol = col + directions[i][1];

		if (neighborRow < 0 || neighborRow >= (int)t->rows) continue;
		if (neighborCol < 0 || neighborCol >= (int)t->columns) continue;

		size_t neighborIdx = getCellIndex(t->columns, neighborRow, neighborCol);
		Cell neighbor = t->cells[neighborIdx];
		if (neighbor.state != ON_FIRE) continue;

		int prop_row = row - neighborRow;
		int prop_col = col - neighborCol;
		
		int weight = basicWeight[abs(prop_row)+abs(prop_col)];
		int alignment = prop_row*w.rowDirection + prop_col*w.columnDirection;
		weight = max(weight + alignment*w.speed, 1);

		totalWeight += weight;
	}

	size_t targetIdx = getCellIndex(t->columns, row, col);
	Cell targetCell = t->cells[targetIdx];

	return (totalWeight*cover_factor[targetCell.cover]*(100 - targetCell.humidity))/100;
}

int simulate(Terrain *t, int *containmentMap, Simulation sim, Wind wind)
{
	size_t nCells = t->rows*t->columns;

	for(size_t i = 0; i < nCells; i++)
		if(t->cells[i].state == INTACT)
			sim.nCombustibles++;
	
	sim.mostIgnitonStep = 0;
	sim.mostIgntionVal = 0;
	sim.stepsTaken = 0;
	sim.totalIgnitons = 0;

	Cell *nextCells = (Cell *) malloc(nCells*sizeof(Cell));
	assert(nextCells);

	clock_t start_time = clock();

	for(int step = 0; step < sim.stepsMax; step++) {
		sim.stepsTaken = step+1;
		int stepIgnitions = 0;
		int stepOnFire = 0;

		// Setup the next state
		memcpy(nextCells, t->cells, nCells*sizeof(Cell));

		for(int i = 0; i < (t->columns*t->rows); i++) {
			// Apply Containments
			if(containmentMap[i] == step && nextCells[i].state == INTACT) {
				nextCells[i].state = CONTAINED;
				continue;
			}

			// Spread Fire
			if(t->cells[i].state == INTACT && ignitionPotential(t, i, wind) > sim.threshold) {
				nextCells[i].state = ON_FIRE;
				nextCells[i].burningTime = initialBurnTime[t->cells[i].cover];
				stepIgnitions++;
				stepOnFire = 1;
			}

			// Extinguiche Fire
			else if(t->cells[i].state == ON_FIRE) {
				stepOnFire = 1;
				if(t->cells[i].burningTime - 1 == 0) {
					nextCells[i].state = BURNT;
					nextCells[i].burningTime = 0; 
				} else {
					nextCells[i].burningTime = t->cells[i].burningTime - 1;
					stepOnFire = 1;
				}
			}
		}

		sim.totalIgnitons += stepIgnitions;
		if(stepIgnitions > sim.mostIgntionVal) {
			sim.mostIgnitonStep = step;
			sim.mostIgntionVal = stepIgnitions;
		}

		memcpy(t->cells, nextCells, nCells*sizeof(Cell));
		if(!stepOnFire) break;
	}

	clock_t end_time = clock();
    double total_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

	free(nextCells);

	int nIntact = 0;
    int nOnFire = 0;
    int nBurnt = 0;
    int nContained = 0;
    int nNonCombustible = 0;

    for (size_t i = 0; i < nCells; i++) {
        switch (t->cells[i].state) {
            case NON_COMBUSTIBLE: nNonCombustible++; break;
            case INTACT:          nIntact++;     break;
            case ON_FIRE:         nOnFire++;     break;
            case BURNT:           nBurnt++;      break;
            case CONTAINED:       nContained++;  break;
        }
    }

	// %
	double burntPercentage = 0.0;
    double protectedPercentage = 0.0;
    if (sim.nCombustibles > 0) {
        burntPercentage = 100.0 * (nBurnt + nOnFire) / sim.nCombustibles;
        protectedPercentage = 100.0 * nContained / sim.nCombustibles;
    }

    // Checksum
    unsigned long long checksum = 0;
    for (size_t i = 0; i < nCells; i++) {
        checksum = checksum * 31ULL + (unsigned long long) t->cells[i].state;
        checksum = checksum * 31ULL + (unsigned long long) t->cells[i].burningTime;
    }

	printf("passos: %hd\n", sim.stepsTaken);
    printf("nao_combustiveis: %d\n", nNonCombustible);
    printf("intactas: %d\n", nIntact);
    printf("em_chamas: %d\n", nOnFire);
    printf("queimadas: %d\n", nBurnt);
    printf("contencao: %d\n", nContained);
    printf("total_ignicoes: %d\n", sim.totalIgnitons);
    printf("pico_ignicoes: %d %d\n", sim.mostIgnitonStep, sim.mostIgntionVal);
	printf("percentual_queimado: %.2f%%\n", burntPercentage);
    printf("percentual_protegido: %.2f%%\n", protectedPercentage);
    printf("checksum: %llu\n", checksum);
    printf("tempo: %.6f\n", total_time);

    return 0;
}
