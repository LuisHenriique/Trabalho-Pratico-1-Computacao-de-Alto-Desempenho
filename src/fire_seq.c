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
        &s->lines, &s->columns, &s->steps, &s->threads, &s->seed, &s->threshold);
    scanf("%hd %hd %hd",
        &w->lineDirection, &w->columnDirection, &w->speed);
    
    int N, M; // number of fire zones and containment zones
    scanf("%d %d", &N, &M);
    
    if(N < 0 || M < 0) 
        error();
    
    *fires = (FireZone *) malloc(sizeof(FireZone)*N);
    assert(*fires);
    
    *containments = (ContainmentZone *) malloc(sizeof(ContainmentZone)*M);
    assert(*containments);

    for(int i = 0; i < N; i++) 
        scanf("%hd %hd", &(*fires)[i].lin, &(*fires)[i].col);

    for(int i = 0; i < M; i++)
        scanf("%hd %hd %hd %hd %hd", 
            &(*containments)[i].step, 
            &(*containments)[i].linX, &(*containments)[i].colX,
            &(*containments)[i].linY, &(*containments)[i].colY);
}

void validate(Simulation s, Wind w, FireZone *fires, ContainmentZone *containments, Cell **terrain) {
    // Simulation
    if(s.lines <= 0 || s.columns <= 0 || s.steps < 0 || s.threads <= 0 || s.threshold <= 0) error();

    // Wind
    if(w.lineDirection < -1 || w.lineDirection > 1) error();
    if(w.columnDirection < -1 || w.columnDirection > 1) error();
    if(!w.lineDirection && !w.columnDirection) error();
    if(w.speed < 0 || w.speed > 5) error();

    // Fires
    size_t N = sizeof(fires) / sizeof(fires[0]);
    for(int i = 0; i < N; i++) {
        if(fires[i].lin < 0 || fires[i].lin > s.lines-1) error();
        if(fires[i].col < 0 || fires[i].col > s.columns-1) error();

        if(terrain[fires[i].lin][fires[i].col].state == 0) error();

        for(int j = i+1; j < N; j++)
            if(fires[i].lin == fires[j].lin && fires[i].col == fires[j].col) error();
    }

    // Containments
    size_t M = sizeof(containments) / sizeof(containments[0]);
    for(int i = 0; i < M; i++) {
        if(containments[i].linX < 0 || containments[i].linX > s.lines-1) error();
        if(containments[i].colX < 0 || containments[i].colX > s.columns-1) error();
        if(containments[i].linY < 0 || containments[i].linY > s.lines-1) error();
        if(containments[i].colY < 0 || containments[i].colY > s.columns-1) error();
        
        if(containments[i].linX > containments[i].linY || containments[i].colX > containments[i].colY) error();
        
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
