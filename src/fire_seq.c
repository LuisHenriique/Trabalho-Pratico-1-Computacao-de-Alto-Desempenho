#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "structs.h"

void readInput(Simulation *s, Wind *w, FireZone *fires, ContainmentZone *containments) {
    s = (Simulation *) malloc(sizeof(Simulation));
    assert(s);
    w = (Wind *) malloc(sizeof(Wind));
    assert(w);


    scanf("%hd %hd %hd %hd %hd %hd",
        &s->lines, &s->columns, &s->steps, &s->threads, &s->seed, &s->threshold);
    scanf("%hd %hd %hd",
        &w->lineDirection, &w->columnDirection, &w->speed);
    
    int N, M; // number of fire zones and contation zones
    scanf("%d %d", &N, &M);

    fires = (FireZone *) malloc(sizeof(FireZone)*N);
    assert(fires);
    containments = (ContainmentZone *) malloc(sizeof(ContainmentZone)*M);
    assert(containments);

    for(int i = 0; i < N; i++) 
        scanf("%hd %hd", &fires[i].lin, &fires[i].col);

    for(int i = 0; i < M; i++)
        scanf("%hd %hd %hd %hd %hd", 
            &containments[i].step, 
            &containments[i].linX, &containments[i].colX,
            &containments[i].linY, &containments[i].colY);
}

int main() {
    Simulation s;
    Wind w;
    FireZone *fires;
    ContainmentZone *containments; 

    readInput(&s, &w, fires, containments);
}
