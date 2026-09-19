#ifndef STRUCT_H
#define STRUCT_H

typedef struct  {
    short rows, columns, steps, threads, threshold;
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
} FireZone;

typedef struct {
    short step, rowX, colX, rowY, colY; 
} ContainmentZone;

typedef struct {
    short cover, humidity, state, burningTime; 
} Cell;

#endif /* STRUCT_H */
