#ifndef SIMULATION_H
#define SIMULATION_H

#include "types.h"
#include "config.h"
#include "graph.h"
#include "dijkstra.h"
#include "linked_list.h"
#include "priority_queue.h"

/*
 * World = the COMPLETE simulation state. No globals anywhere in the project.
 * The map renderer (map.c) and main.c both operate on this SAME state, so the
 * display can never diverge from the algorithm data.
 *
 * Phase 2 will add an EventQueue (FIFO scheduled events) INSIDE this struct -
 * adding it later requires zero changes to any existing field.
 */
typedef struct World {
    Graph graph;
    List roads;
    List ambulances;
    List hospitals;
    List emergencies;
    List log;                 /* LogEntry records (report.h) */
    PriorityQueue pq;         /* pending emergencies (binary heap) */
    long clock;               /* simulation minutes */
    int nextEmergencyId;
    int autoDispatch;         /* dispatch automatically each tick */
    int verbose;              /* print log lines to stdout */
    int dispatched, completed, failed, reroutes;
} World;

int  world_init(World *w);
void world_free(World *w);
int  world_loadData(World *w, const char *nodesCsv, const char *roadsCsv,
                    const char *ambulancesCsv, const char *hospitalsCsv);
int  world_loadEmergencies(World *w, const char *csv);   /* optional pre-queued calls */
Emergency *world_createEmergency(World *w, int locationNode, Severity sev,
                                 EmergencyType type, int patients);

void simulation_tick(World *w);                /* advance exactly 1 minute */
void simulation_runFor(World *w, long minutes);
void simulation_runUntilSettled(World *w, long maxMinutes);

#endif
