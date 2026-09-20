#include "simulation.h"
#include "terrain.h"
#include "structs.h"
#include <stdlib.h>
#include <string.h>

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
