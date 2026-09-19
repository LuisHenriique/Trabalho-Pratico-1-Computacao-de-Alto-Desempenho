#include <stdio.h>
#include <stdlib.h>
#include "structs.h"

readInput(Simulation *s, Wind *w, FireZone *fires, ContationZone *contations) {
    scanf("%hd %hd %hd %hd %hd %hd",
        s->lines, s->columns, s->steps, s->threads, s->seed, s->limiar);
    scanf("%hd %hd %hd",
        w->lineDirection, w->columnDirection, w->speed);
    
    int N, M; // number of fire zones and contation zones
    scanf("%d %d", &N, &M);

    fires = malloc(sizeof(FireZone)*N);
    contations = malloc(sizeof(ContationZone)*M);
    for(int i = 0; i < N; i++) 
        scanf("%hd %hd", fires[i].lin, fires[i].col);

    for(int i = 0; i < M; i++)
        scanf("%hd %hd %hd %hd %hd", 
            contations[i].step, 
            contations[i].linX, contations[i].colX,
            contations[i].linY, contations[i].colY);
}

int main() {
    Simulation s;
    Wind w;
    FireZone *fires;
    ContationZone *contations; 

    readInput(&s, &w, fires, contations);
}