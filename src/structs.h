#ifndef STRUCT_H
#define STRUCT_H

#include <stddef.h>

typedef struct  {
    short stepsMax, stepsTaken, threads, threshold;
    int nCombustibles, mostIgntionVal, mostIgnitonStep, totalIgnitons;
    unsigned seed;
} Simulation;

// Line is North-South (-1, 1) and Column is Weast-East(-1,1)
typedef struct {
    short rowDirection;
    short columnDirection;
    short speed;
} Wind;

typedef struct {
    short row, col;
} FirePos;

typedef struct {
    short step, rowX, colX, rowY, colY; 
} ContainmentZone;


typedef struct {
    size_t count;
    ContainmentZone data[];
} ContainmentZoneArray;

typedef struct {
	size_t count;
	FirePos data[];
} FirePosArray;

#endif /* STRUCT_H */
