#ifndef STRUCT_H
#define STRUCT_H

typedef struct  {
    short lines, columns, steps, threads, threshold;
    unsigned seed;
} Simulation;

// Line is North-South (-1, 1) and Column is Weast-East(-1,1)
typedef struct {
    short lineDirection;
    short columnDirection;
    short speed;
} Wind;

typedef struct {
    short lin, col;
} FireZone;

typedef struct {
    short step, linX, colX, linY, colY; 
} ContainmentZone;

typedef struct {
    short cover, humidity, state, burningTime; 
} Cell;

#endif /* STRUCT_H */
