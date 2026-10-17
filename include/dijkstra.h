#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include "types.h"
#include "graph.h"

typedef struct Route {
    int success;
    int totalCost;
    int *path;        /* path[0] = source ... path[pathLength-1] = destination */
    int pathLength;
} Route;

Route dijkstra_shortestPath(const Graph *g, int source, int destination);
void  route_free(Route *r);

/* Helpers for the dynamic re-routing logic (Batch 3 me use honge) */
int   route_costFrom(const Graph *g, const int *path, int len, int startEdge);
int   route_hasBlockedEdge(const Graph *g, const int *path, int len, int startEdge);
void  route_toString(const int *path, int len, char *buf, int size);

#endif
