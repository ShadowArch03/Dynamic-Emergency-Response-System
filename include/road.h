#ifndef ROAD_H
#define ROAD_H

#include "types.h"
#include "graph.h"
#include "linked_list.h"

/*
 * Road registry = MASTER record. Graph edges are kept in sync with it
 * (road_syncGraph), so Dijkstra always sees CURRENT traffic reality.
 * Return codes: 0 = changed, 1 = no change possible, -1 = not found.
 */
Road *road_add(List *roads, Graph *g, int id, int from, int to, int baseTime);
Road *road_findById(const List *roads, int id);
Road *road_findByNodes(const List *roads, int from, int to);
int  road_setCondition(List *roads, Graph *g, int roadId, RoadCondition cond);
int  road_block(List *roads, Graph *g, int roadId);
int  road_open(List *roads, Graph *g, int roadId);
int  road_increaseTraffic(List *roads, Graph *g, int roadId);
int  road_decreaseTraffic(List *roads, Graph *g, int roadId);
RoadCondition road_conditionOf(const Road *r);
int  road_countByCondition(const List *roads, RoadCondition cond);
const char *road_conditionName(RoadCondition c);
void road_printAll(const List *roads);

#endif
