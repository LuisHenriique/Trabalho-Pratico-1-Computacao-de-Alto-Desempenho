#include "terrain.h"
#include <assert.h>
#include <stdlib.h>

short getCover(int rand_val)
{
    for (int i = 0; i < N_COVERS; i++)
        if (rand_val >= numberToCover[i].min_val && rand_val <= numberToCover[i].max_val)
            return numberToCover[i].code;
    return -1;
}

void generateTerrainMatrix(Terrain *terrain, unsigned *seed)
{
    for(int i = 0; i < (terrain->rows*terrain->columns); i++) {
        int rand_val = rand_r(seed) % 100;
        terrain->cells[i].cover = getCover(rand_val);
        // Check for -1

        terrain->cells[i].humidity = rand_r(seed) % 101;
        terrain->cells[i].state = initial_state[terrain->cells[i].cover];
    }
}

size_t getCellIndex(size_t nCollumns, size_t row, size_t column)
{
	return nCollumns*row + column;
}
