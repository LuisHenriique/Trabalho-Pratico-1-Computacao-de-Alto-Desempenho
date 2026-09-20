#include "simulation.h"
#include "terrain.h"
#include "structs.h"
#include <stdlib.h>
#include <string.h> 
#include <assert.h>

int max(int a, int b)
{
	if (a > b) return a;
	return b;
}

void startFire(Terrain *t, FirePosArray *fires)
{
	for (long i=0; i<fires->count; i++)
	{
		// Double check if validation guarantees firePos is forest or undergrowth 
		size_t index = getCellIndex(t->columns, fires->data[i].row, fires->data[i].col);
		t->cells[index].state = ON_FIRE;
		t->cells[index].burningTime = initialBurnTime[t->cells[index].cover];
	}
}

int *createContainmentMap(ContainmentZoneArray *containments, size_t rows, size_t columns)
{
	size_t nCells = rows * columns;
	int *containmentMap = (int *) malloc(sizeof(int)*nCells);
	memset(containmentMap, -1, sizeof(int)*nCells);

	for (int i=0; i< containments->count; i++)
	{
		ContainmentZone zone = containments->data[i];
		short step = containments->data[i].step;

		for (int j=zone.rowX; j<zone.rowY; j++)
		{
			for (int k=zone.colX; k<zone.colY; k++) {
				size_t index = getCellIndex(columns, j, k);
				if(containmentMap[index] == -1)
					containmentMap[index] = step;
				else if (containmentMap[index] > step)
					containmentMap[index] = step;
			}
		}
	}

	return containmentMap;
}

int ignitionPotential(Terrain *t, int index, Wind w)
{
	char directions[8][2] = {{-1,-1}, {-1,0}, {-1,1},
							  {0, -1} 		    ,{0, 1},
							  {1, -1}, {1, 0}, {1, 1}};

	int row = index/t->rows;
	int col = index - row*t->columns;

	int totalWeight = 0;

	for (int i=0; i<8; i++) {
		int neighborRow = row + directions[i][0];
		int neighborCol = col + directions[i][1];

		if (neighborRow < 0 || neighborRow >= t->rows) continue;
		if (neighborCol < 0 || neighborCol >= t->columns) continue;

		int neighborIdx = neighborRow*t->columns+neighborCol;
		Cell neighbor = t->cells[neighborIdx];
		int prop_row = row - neighborRow;
		int prop_column = col - neighborCol;

		int weight = basicWeight[abs(prop_row)+abs(prop_column)];
		int alignment = prop_row*w.rowDirection+prop_column*w.columnDirection;
		weight = max(weight + alignment*w.speed, 1);

		totalWeight += weight;
	}

	Cell targetCell = t->cells[row*t->columns+col];

	return (totalWeight*cover_factor[targetCell.cover]*(100 - targetCell.humidity))/100;
}

int notBurning(Cell *cells, int nCells)
{
	for (int i=0; i<nCells; i++)
		if (cells[i].state == ON_FIRE) return 0;
	return 1;
}

int simulate(Terrain *t, int *containmentMap, Simulation sim, Wind wind)
{
	int nCells = t->rows*t->columns;

	Cell *newCells = (Cell *) malloc(nCells*sizeof(Cell));
	assert(newCells);

	memcpy(newCells, t->cells, nCells*sizeof(Cell));

	for(int step=0; step<sim.steps; step++) {
		for(int i=0; i<t->columns * t->rows; i++) {
			if(containmentMap[i] == step && newCells[i].state == INTACT)
				newCells[i].state = CONTAINED;

			if (newCells[i].state == INTACT && 
				ignitionPotential(t, i, wind) > sim.threshold)
			{
				newCells[i].state = ON_FIRE;
				newCells[i].burningTime = initialBurnTime[newCells[i].cover];
			}
		}

		memcpy(t->cells, newCells, nCells*sizeof(Cell));
		if(notBurning(t->cells, nCells)) break;
	}

	free(newCells);
}
