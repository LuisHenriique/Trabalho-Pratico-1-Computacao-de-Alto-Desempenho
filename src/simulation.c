#include "simulation.h"
#include <assert.h>
#include <stdlib.h>

Cell *createTerrainMatrix(Simulation sim);


void simulate(Simulation s, Wind w, FireZone *fires, ContainmentZone *containments) {
    Cell *terrain = (Cell *) createTerrainMatrix(s);
}

short getCover(int rand_val)
{
    for (int i = 0; i < N_COVERS; i++)
        if (rand_val >= numberToCover[i].min_val && 
            rand_val <= numberToCover[i].max_val)
            return numberToCover[i].code;
    return -1;
}

Cell *createTerrainMatrix(Simulation sim)
{
    Cell *terrain = (Cell *) malloc(sizeof(Cell)*sim.lines*sim.columns);
    assert(terrain);

    for(int i=0; i<sim.lines*sim.columns;i++)
    {
        int rand_val = rand_r(&sim.seed) % 100;
        terrain[i].cover = getCover(rand_val);
        // Check for -1

        terrain[i].humidity = rand_r(&sim.seed) % 101;
        terrain[i].state = initial_state[terrain[i].cover];
    }
}
