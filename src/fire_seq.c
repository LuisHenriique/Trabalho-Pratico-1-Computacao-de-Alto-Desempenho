#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "structs.h"
#include "simulation.h"

void error() {
    printf("Mensagem de erro\n");
    exit(1);
}

void readInput(Simulation *s, Wind *w, FireZone **fires, ContainmentZone **containments) {

    scanf("%hd %hd %hd %hd %hd %hd",
        &s->rows, &s->columns, &s->steps, &s->threads, &s->seed, &s->threshold);
    scanf("%hd %hd %hd",
        &w->rowDirection, &w->columnDirection, &w->speed);
    
    int N, M; // number of fire zones and containment zones
    scanf("%d %d", &N, &M);
    
    if(N < 0 || M < 0) 
        error();
    
    *fires = (FireZone *) malloc(sizeof(FireZone)*N);
    assert(*fires);
    
    *containments = (ContainmentZone *) malloc(sizeof(ContainmentZone)*M);
    assert(*containments);

    for(int i = 0; i < N; i++) 
        scanf("%hd %hd", &(*fires)[i].row, &(*fires)[i].col);

    for(int i = 0; i < M; i++)
        scanf("%hd %hd %hd %hd %hd", 
            &(*containments)[i].step, 
            &(*containments)[i].rowX, &(*containments)[i].colX,
            &(*containments)[i].rowY, &(*containments)[i].colY);
}

void validate(Simulation s, Wind w, FireZone *fires, ContainmentZone *containments, Cell **terrain) {
    // Simulation
    if(s.rows <= 0 || s.columns <= 0 || s.steps < 0 || s.threads <= 0 || s.threshold <= 0) error();

    // Wind
    if(w.rowDirection < -1 || w.rowDirection > 1) error();
    if(w.columnDirection < -1 || w.columnDirection > 1) error();
    if(!w.rowDirection && !w.columnDirection) error();
    if(w.speed < 0 || w.speed > 5) error();

    // Fires
    size_t N = sizeof(fires) / sizeof(fires[0]);
    for(int i = 0; i < N; i++) {
        if(fires[i].row < 0 || fires[i].row > s.rows-1) error();
        if(fires[i].col < 0 || fires[i].col > s.columns-1) error();

        if(terrain[fires[i].row][fires[i].col].state == 0) error();

        for(int j = i+1; j < N; j++)
            if(fires[i].row == fires[j].row && fires[i].col == fires[j].col) error();
    }

    // Containments
    size_t M = sizeof(containments) / sizeof(containments[0]);
    for(int i = 0; i < M; i++) {
        if(containments[i].rowX < 0 || containments[i].rowX > s.rows-1) error();
        if(containments[i].colX < 0 || containments[i].colX > s.columns-1) error();
        if(containments[i].rowY < 0 || containments[i].rowY > s.rows-1) error();
        if(containments[i].colY < 0 || containments[i].colY > s.columns-1) error();
        
        if(containments[i].rowX > containments[i].rowY || containments[i].colX > containments[i].colY) error();
        
        if(containments[i].step < 0 || containments[i].step >= s.steps) error();
    }
}


int main() {
    Simulation s;
    Wind w;
    FireZone *fires;
    ContainmentZone *containments; 

    readInput(&s, &w, &fires, &containments);
    simulate(s, w, fires, containments);

    return EXIT_SUCCESS;
}
