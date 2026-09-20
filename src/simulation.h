#include <stddef.h>
#include "terrain.h"

int simulate(Terrain *t, int *containmentMap, Simulation sim, Wind wind);
int *createContainmentMap(ContainmentZoneArray *containments, size_t rows, size_t columns);
void startFire(Terrain *t, FirePosArray *fires);
