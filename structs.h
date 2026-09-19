typedef struct  {
    short lines, columns, steps, threads, seed, limiar;
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
} ContationZone;

typedef struct {
    short cover, moisty, state, burningTime; 
} Cell;