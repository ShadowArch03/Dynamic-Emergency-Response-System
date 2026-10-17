#include <stdio.h>
#include <stdlib.h>
#include "road.h"

/*
 * Condition model (4 levels):
 *   NORMAL  -> currentTime = baseTime * 1
 *   TRAFFIC -> currentTime = baseTime * 2
 *   HEAVY   -> currentTime = baseTime * 3
 *   BLOCKED -> edge ignored by Dijkstra entirely
 */
static void road_syncGraph(Graph *g, const Road *r) {
    graph_setEdgeBlocked(g, r->from, r->to, r->blocked);
    if (!r->blocked)
        graph_setEdgeWeight(g, r->from, r->to, r->currentTime);
}

Road *road_add(List *roads, Graph *g, int id, int from, int to, int baseTime) {
    if (!roads || !g || baseTime <= 0) return NULL;
    if (from < 0 || from >= g->numNodes || to < 0 || to >= g->numNodes) return NULL;
    if (from == to) return NULL;
    if (road_findByNodes(roads, from, to)) return NULL;   /* duplicate road */

    Road *r = calloc(1, sizeof(Road));
    if (!r) return NULL;
    r->id = id;
    r->from = from;
    r->to = to;
    r->baseTime = baseTime;
    r->currentTime = baseTime;
    r->blocked = 0;

    if (graph_addEdge(g, from, to, baseTime) != 0) { free(r); return NULL; }
    if (list_push_back(roads, r) != 0) { free(r); return NULL; }
    return r;
}

Road *road_findById(const List *roads, int id) {
    for (ListNode *n = roads->head; n; n = n->next)
        if (((Road *)n->data)->id == id) return (Road *)n->data;
    return NULL;
}

Road *road_findByNodes(const List *roads, int from, int to) {
    for (ListNode *n = roads->head; n; n = n->next) {
        Road *r = (Road *)n->data;
        if ((r->from == from && r->to == to) || (r->from == to && r->to == from))
            return r;
    }
    return NULL;
}

int road_setCondition(List *roads, Graph *g, int roadId, RoadCondition cond) {
    Road *r = road_findById(roads, roadId);
    if (!r) return -1;
    switch (cond) {
        case ROAD_NORMAL:  r->blocked = 0; r->currentTime = r->baseTime;     break;
        case ROAD_TRAFFIC: r->blocked = 0; r->currentTime = r->baseTime * 2; break;
        case ROAD_HEAVY:   r->blocked = 0; r->currentTime = r->baseTime * 3; break;
        case ROAD_BLOCKED: r->blocked = 1;                                   break;
        default: return -1;
    }
    road_syncGraph(g, r);
    return 0;
}

int road_block(List *roads, Graph *g, int roadId) {
    return road_setCondition(roads, g, roadId, ROAD_BLOCKED);
}

int road_open(List *roads, Graph *g, int roadId) {
    return road_setCondition(roads, g, roadId, ROAD_NORMAL);
}

/* Traffic rises only up to HEAVY; blocking is a SEPARATE manual action. */
int road_increaseTraffic(List *roads, Graph *g, int roadId) {
    Road *r = road_findById(roads, roadId);
    if (!r) return -1;
    RoadCondition c = road_conditionOf(r);
    if (c == ROAD_BLOCKED || c == ROAD_HEAVY) return 1;
    return road_setCondition(roads, g, roadId, (RoadCondition)(c + 1));
}

/* Traffic falls only down to NORMAL; unblocking is a SEPARATE manual action. */
int road_decreaseTraffic(List *roads, Graph *g, int roadId) {
    Road *r = road_findById(roads, roadId);
    if (!r) return -1;
    RoadCondition c = road_conditionOf(r);
    if (c == ROAD_NORMAL || c == ROAD_BLOCKED) return 1;
    return road_setCondition(roads, g, roadId, (RoadCondition)(c - 1));
}

RoadCondition road_conditionOf(const Road *r) {
    if (r->blocked) return ROAD_BLOCKED;
    if (r->currentTime >= r->baseTime * 3) return ROAD_HEAVY;
    if (r->currentTime >= r->baseTime * 2) return ROAD_TRAFFIC;
    return ROAD_NORMAL;
}

int road_countByCondition(const List *roads, RoadCondition cond) {
    int count = 0;
    for (ListNode *n = roads->head; n; n = n->next)
        if (road_conditionOf((Road *)n->data) == cond) count++;
    return count;
}

const char *road_conditionName(RoadCondition c) {
    switch (c) {
        case ROAD_NORMAL:  return "NORMAL";
        case ROAD_TRAFFIC: return "TRAFFIC";
        case ROAD_HEAVY:   return "HEAVY";
        default:           return "BLOCKED";
    }
}

void road_printAll(const List *roads) {
    printf("%-4s %-8s %-6s %-9s %-8s %s\n",
           "ID", "FROM-TO", "BASE", "CURRENT", "BLOCKED", "STATE");
    for (ListNode *n = roads->head; n; n = n->next) {
        Road *r = (Road *)n->data;
        printf("%-4d %d-%-6d %-6d %-9d %-8s %s\n",
               r->id, r->from, r->to, r->baseTime, r->currentTime,
               r->blocked ? "YES" : "no",
               road_conditionName(road_conditionOf(r)));
    }
}
