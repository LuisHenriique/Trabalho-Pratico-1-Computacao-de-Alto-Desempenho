#ifndef TERRAIN_H
#define TERRAIN_H

#include "structs.h"

#define N_COVERS 4

typedef enum {
    WATER,
    EXPOSED_GROUND,
    UNDERGROWTH,
    FOREST,
    CODE_COUNT
} Cover;

typedef enum {
    NON_COMBUSTIBLE,
    INTACT,
    ON_FIRE,
    BURNT,
    CONTAINED,
} State;

typedef enum {
	SELF,
	ORTHOGONAL,
	DIAGONAL
}Directions;

typedef struct {
    int min_val;
    int max_val;
    char code;
} NumberCoverMap;

typedef struct {
    short cover, humidity, state, burningTime; 
} Cell;

typedef struct {
	size_t rows, columns;
	Cell cells[];
} Terrain;

static const NumberCoverMap numberToCover[] = {
    /* min   max    code */
    {  0,     9,    WATER,  },
    { 10,    19,    EXPOSED_GROUND,  },
    { 20,    54,    UNDERGROWTH,  },
    { 55,    99,    FOREST,  },
};

static const int cover_factor[] = {
    [WATER] = 0,
    [EXPOSED_GROUND] = 0,
    [UNDERGROWTH] = 8,
    [FOREST] = 12 
};

static const int initial_state[] = {
    [WATER] = NON_COMBUSTIBLE,
    [EXPOSED_GROUND] = NON_COMBUSTIBLE,
    [UNDERGROWTH] = INTACT,
    [FOREST] = INTACT,
};

static const int initialBurnTime[] = {
    [UNDERGROWTH] = 2,
    [FOREST] = 4 
};

static const int basicWeight[] = {
	[ORTHOGONAL] = 10,
	[DIAGONAL] = 7,
};

//void simulate(Simulation s, Wind w, FirePosArray *fires, ContainmentZoneArray *containments);
void generateTerrainMatrix(Terrain *terrain, unsigned *seed);
size_t getCellIndex(size_t nCollumns, size_t row, size_t column);

#endif /* TERRAIN_H */
