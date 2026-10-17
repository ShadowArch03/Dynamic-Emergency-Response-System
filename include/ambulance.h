#ifndef AMBULANCE_H
#define AMBULANCE_H

#include "types.h"
#include "graph.h"
#include "dijkstra.h"
#include "linked_list.h"

Ambulance *ambulance_create(List *ambulances, int id, int currentNode,
                            AmbulanceStatus status, AmbulanceType type);
Ambulance *ambulance_findById(const List *ambulances, int id);
int  ambulance_countByStatus(const List *ambulances, AmbulanceStatus status);
void ambulance_clearRoute(Ambulance *a);
void ambulance_release(Ambulance *a);
int  ambulance_startRoute(Ambulance *a, const Graph *g, const Route *r);
void ambulance_printAll(const List *ambulances);
const char *ambulanceStatus_name(AmbulanceStatus s);
const char *ambulanceType_name(AmbulanceType t);

#endif
