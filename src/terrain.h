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
    CONTENTION,
} State;


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

//void simulate(Simulation s, Wind w, FirePosArray *fires, ContainmentZoneArray *containments);
Cell *generateTerrainMatrix(Terrain *terrain, unsigned seed);
Cell getCell(Terrain *t, size_t row, size_t column);
