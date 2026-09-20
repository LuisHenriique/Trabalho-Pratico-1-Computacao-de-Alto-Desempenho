#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "structs.h"
#include "terrain.h"
#include "simulation.h"

void error() {
    printf("Mensagem de erro\n");
    exit(1);
}

void readInput(FILE *f, Simulation *s, Wind *w, FirePosArray **firesAddr, ContainmentZoneArray **containmentsAddr, Terrain **terrain) {
	size_t terrain_rows, terrain_columns;

    fscanf(f, "%zu %zu %hd %hd %u %hd",
        &terrain_rows, &terrain_columns, &s->stepsMax, &s->threads, &s->seed, &s->threshold);
    fscanf(f, "%hd %hd %hd",
        &w->rowDirection, &w->columnDirection, &w->speed);


	*terrain = (Terrain *) malloc(sizeof(Terrain) + terrain_rows*terrain_columns*sizeof(Cell));
	assert(terrain);
	(*terrain)->rows = terrain_rows;
	(*terrain)->columns = terrain_columns;

    size_t N, M; // Number of fire zones and containment zones
    fscanf(f, "%zu %zu", &N, &M);
    
    if(N < 0 || M < 0) 
        error();
    
    *firesAddr = (FirePosArray *) malloc(sizeof(FirePosArray) + N*sizeof(FirePos));
    assert(*firesAddr);
    
    *containmentsAddr = (ContainmentZoneArray *) malloc(sizeof(ContainmentZoneArray) + M*sizeof(ContainmentZone));
    assert(*containmentsAddr);

	FirePosArray *fires = *firesAddr;
	ContainmentZoneArray *containments = *containmentsAddr;
    fires->count = N;
    containments->count = M;

    for(int i = 0; i < N; i++) {
		FirePos *curr_pos = &fires->data[i];
        fscanf(f, "%hd %hd", &curr_pos->row, &curr_pos->col);
	}	

    for(int i = 0; i < M; i++) {
		ContainmentZone *curr_zone = &containments->data[i];
        fscanf(f, "%hd %hd %hd %hd %hd", &curr_zone->step, &curr_zone->rowX, &curr_zone->colX,
            						 &curr_zone->rowY, &curr_zone->colY);
	}
}

void validate(Simulation s, Wind w, FirePosArray *fires, ContainmentZoneArray *containments, Terrain *terrain) {
    // Simulation
    if(terrain->rows <= 0 || terrain->columns <= 0 || s.stepsMax < 0 || s.threads <= 0 || s.threshold <= 0) error();

    // Wind
    if(w.rowDirection < -1 || w.rowDirection > 1) error();
    if(w.columnDirection < -1 || w.columnDirection > 1) error();
    if(!w.rowDirection && !w.columnDirection) error();
    if(w.speed < 0 || w.speed > 5) error();

    // Fires
    size_t N = fires->count; 
    for(int i = 0; i < N; i++) {
        if(fires->data[i].row < 0 || fires->data[i].row > terrain->rows-1) error();
        if(fires->data[i].col < 0 || fires->data[i].col > terrain->columns-1) error();

		size_t ci = getCellIndex(terrain->columns, fires->data[i].row, fires->data[i].col);
		if(terrain->cells[ci].state == 0) error();

        for(int j = i+1; j < N; j++)
            if(fires->data[i].row == fires->data[j].row && fires->data[i].col == fires->data[j].col) error();
    }

    // Containments
    size_t M = containments->count;
    for(int i = 0; i < M; i++) {
        if(containments->data[i].rowX < 0 || containments->data[i].rowX > terrain->rows-1) error();
        if(containments->data[i].colX < 0 || containments->data[i].colX > terrain->columns-1) error();
        if(containments->data[i].rowY < 0 || containments->data[i].rowY > terrain->rows-1) error();
        if(containments->data[i].colY < 0 || containments->data[i].colY > terrain->columns-1) error();
        
        if(containments->data[i].rowX > containments->data[i].rowY || containments->data[i].colX > containments->data[i].colY) error();
        
        if(containments->data[i].step < 0 || containments->data[i].step >= s.stepsMax) error();
    }
}

int main(int argc, char *argv[]) {
    if(argc != 2) error();

    FILE *f = fopen(argv[1], "r");
    if(!f) error();

    Simulation sim;
    Wind wind;
    FirePosArray *fires;
    ContainmentZoneArray *containments; 
	Terrain *t;
	int *containmentMap;

    readInput(f, &sim, &wind, &fires, &containments, &t);
    fclose(f);

    generateTerrainMatrix(t, &sim.seed);
	validate(sim, wind, fires, containments, t);
	startFire(t, fires);
	containmentMap = createContainmentMap(containments, t->rows, t->columns);

	simulate(t, containmentMap, sim, wind);

    free(containmentMap);
    free(fires);
    free(containments);
    free(t);

    return EXIT_SUCCESS;
}
