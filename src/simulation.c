#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simulation.h"
#include "dispatch.h"
#include "emergency.h"
#include "ambulance.h"
#include "hospital.h"
#include "road.h"
#include "report.h"

static void ambulance_dtor(void *data) {
    Ambulance *a = (Ambulance *)data;
    free(a->route);                 /* route array is owned by the ambulance */
    free(a);
}

int world_init(World *w) {
    memset(w, 0, sizeof *w);
    if (graph_init(&w->graph, 32) != 0) return -1;
    list_init(&w->roads);
    list_init(&w->ambulances);
    list_init(&w->hospitals);
    list_init(&w->emergencies);
    list_init(&w->log);
    pq_init(&w->pq);
    w->clock = 0;
    w->nextEmergencyId = 1;
    w->autoDispatch = 1;
    w->verbose = 1;
    return 0;
}

void world_free(World *w) {
    graph_free(&w->graph);
    list_clear(&w->roads, free);
    list_clear(&w->hospitals, free);
    list_clear(&w->emergencies, free);
    list_clear(&w->log, free);
    list_clear(&w->ambulances, ambulance_dtor);
    pq_free(&w->pq);
}

/* ------------------------- CSV loading ------------------------- */

/* Splits a CSV line in-place. Returns the field count. */
static int splitLine(char *line, char **fields, int maxFields) {
    int count = 0;
    char *start = line;
    for (char *p = line; count < maxFields; p++) {
        if (*p == ',' || *p == '\n' || *p == '\r' || *p == '\0') {
            char saved = *p;
            *p = '\0';
            fields[count++] = start;
            if (saved == '\0' || saved == '\n' || saved == '\r') break;
            start = p + 1;
        }
    }
    return count;
}

static NodeType parseNodeType(const char *s) {
    if (strcmp(s, "HOSPITAL") == 0) return NODE_HOSPITAL;
    if (strcmp(s, "STATION") == 0)  return NODE_STATION;
    return NODE_JUNCTION;
}

static AmbulanceStatus parseAmbStatus(const char *s) {
    if (strcmp(s, "BUSY") == 0)         return AMB_BUSY;
    if (strcmp(s, "MAINTENANCE") == 0)  return AMB_MAINTENANCE;
    return AMB_AVAILABLE;
}

static AmbulanceType parseAmbType(const char *s) {
    return strcmp(s, "ICU") == 0 ? AMB_ICU : AMB_GENERAL;
}

static Severity parseSeverity(const char *s) {
    if (strcmp(s, "MEDIUM") == 0)   return SEV_MEDIUM;
    if (strcmp(s, "HIGH") == 0)     return SEV_HIGH;
    if (strcmp(s, "CRITICAL") == 0) return SEV_CRITICAL;
    return SEV_LOW;
}

static EmergencyType parseEmergencyType(const char *s) {
    return strcmp(s, "ICU") == 0 ? EMG_NEED_ICU : EMG_NEED_GENERAL;
}

static int loadNodes(World *w, const char *path) {
    FILE *f = fopen(path, "r");
    char line[512];
    char *fields[16];
    if (!f) { fprintf(stderr, "Cannot open %s\n", path); return -1; }
    if (!fgets(line, sizeof line, f)) { fclose(f); return -1; }    /* skip header */
    while (fgets(line, sizeof line, f)) {
        int n = splitLine(line, fields, 16);
        if (n < 5 || fields[0][0] == '\0') continue;
        int id = atoi(fields[0]);
        if (id != w->graph.numNodes) {                             /* enforce 0-based sequential */
            fprintf(stderr, "%s: node ids must be 0-based sequential (expected %d, got %d)\n",
                    path, w->graph.numNodes, id);
            fclose(f);
            return -1;
        }
        if (graph_addNode(&w->graph, fields[1], atoi(fields[3]), atoi(fields[4]),
                          parseNodeType(fields[2])) < 0) { fclose(f); return -1; }
    }
    fclose(f);
    return w->graph.numNodes > 0 ? 0 : -1;
}

static int loadRoads(World *w, const char *path) {
    FILE *f = fopen(path, "r");
    char line[512];
    char *fields[16];
    if (!f) { fprintf(stderr, "Cannot open %s\n", path); return -1; }
    if (!fgets(line, sizeof line, f)) { fclose(f); return -1; }
    while (fgets(line, sizeof line, f)) {
        int n = splitLine(line, fields, 16);
        if (n < 4 || fields[0][0] == '\0') continue;
        int id = atoi(fields[0]), from = atoi(fields[1]), to = atoi(fields[2]), base = atoi(fields[3]);
        Road *r = road_add(&w->roads, &w->graph, id, from, to, base);
        if (!r) {
            fprintf(stderr, "%s: invalid road entry (id %d)\n", path, id);
            fclose(f);
            return -1;
        }
        if (n >= 6 && atoi(fields[5]) == 1) {
            road_setCondition(&w->roads, &w->graph, id, ROAD_BLOCKED);
        } else if (n >= 5) {
            int cur = atoi(fields[4]);
            if (cur > 0 && cur != base) {                          /* custom live weight */
                r->currentTime = cur;
                graph_setEdgeWeight(&w->graph, r->from, r->to, cur);
            }
        }
    }
    fclose(f);
    return 0;
}

static int loadAmbulances(World *w, const char *path) {
    FILE *f = fopen(path, "r");
    char line[512];
    char *fields[16];
    if (!f) { fprintf(stderr, "Cannot open %s\n", path); return -1; }
    if (!fgets(line, sizeof line, f)) { fclose(f); return -1; }
    while (fgets(line, sizeof line, f)) {
        int n = splitLine(line, fields, 16);
        if (n < 4 || fields[0][0] == '\0') continue;
        int id = atoi(fields[0]), node = atoi(fields[1]);
        if (node < 0 || node >= w->graph.numNodes) {
            fprintf(stderr, "%s: ambulance %d has invalid node %d\n", path, id, node);
            fclose(f);
            return -1;
        }
        if (!ambulance_create(&w->ambulances, id, node,
                              parseAmbStatus(fields[2]), parseAmbType(fields[3]))) {
            fclose(f);
            return -1;
        }
    }
    fclose(f);
    return 0;
}

static int loadHospitals(World *w, const char *path) {
    FILE *f = fopen(path, "r");
    char line[512];
    char *fields[16];
    if (!f) { fprintf(stderr, "Cannot open %s\n", path); return -1; }
    if (!fgets(line, sizeof line, f)) { fclose(f); return -1; }
    while (fgets(line, sizeof line, f)) {
        int n = splitLine(line, fields, 16);
        if (n < 6 || fields[0][0] == '\0') continue;
        int id = atoi(fields[0]), node = atoi(fields[1]);
        if (node < 0 || node >= w->graph.numNodes) {
            fprintf(stderr, "%s: hospital %d has invalid node %d\n", path, id, node);
            fclose(f);
            return -1;
        }
        if (!hospital_create(&w->hospitals, id, node, fields[2],
                             atoi(fields[3]), atoi(fields[4]), atoi(fields[5]))) {
            fclose(f);
            return -1;
        }
    }
    fclose(f);
    return 0;
}

int world_loadData(World *w, const char *nodesCsv, const char *roadsCsv,
                   const char *ambulancesCsv, const char *hospitalsCsv) {
    if (loadNodes(w, nodesCsv) != 0) return -1;
    if (loadRoads(w, roadsCsv) != 0) return -1;
    if (loadAmbulances(w, ambulancesCsv) != 0) return -1;
    if (loadHospitals(w, hospitalsCsv) != 0) return -1;
    return 0;
}

/* Optional pre-queued emergencies. Returns how many were loaded. */
int world_loadEmergencies(World *w, const char *path) {
    FILE *f = fopen(path, "r");
    char line[512];
    char *fields[16];
    int loaded = 0;
    if (!f) return 0;
    if (!fgets(line, sizeof line, f)) { fclose(f); return 0; }
    while (fgets(line, sizeof line, f)) {
        int n = splitLine(line, fields, 16);
        if (n < 5 || fields[0][0] == '\0') continue;
        int loc = atoi(fields[1]);
        if (loc < 0 || loc >= w->graph.numNodes) continue;         /* skip bad row */
        Emergency *e = emergency_create(&w->emergencies, w->nextEmergencyId, loc,
                                        parseSeverity(fields[2]),
                                        parseEmergencyType(fields[3]),
                                        atoi(fields[4]), w->clock);
        if (e) {
            pq_push(&w->pq, e->id, e->severity, e->createdAt);
            w->nextEmergencyId++;
            loaded++;
        }
    }
    fclose(f);
    return loaded;
}

Emergency *world_createEmergency(World *w, int locationNode, Severity severity,
                                 EmergencyType type, int patients) {
    if (locationNode < 0 || locationNode >= w->graph.numNodes) return NULL;
    Emergency *e = emergency_create(&w->emergencies, w->nextEmergencyId, locationNode,
                                    severity, type, patients, w->clock);
    if (!e) return NULL;
    pq_push(&w->pq, e->id, e->severity, e->createdAt);
    w->nextEmergencyId++;
    Node *node = graph_getNode(&w->graph, locationNode);
    report_log(w, "NEW EMERGENCY E%d at %s (node %d): %s %s, %d patient(s)",
               e->id, node ? node->name : "?", locationNode,
               severity_name(severity), emergencyType_name(type), e->patients);
    return e;
}

/* ------------------------- simulation engine ------------------------- */

static void moveAmbulances(World *w) {
    for (ListNode *n = w->ambulances.head; n; n = n->next) {
        Ambulance *amb = (Ambulance *)n->data;
        if (amb->status != AMB_BUSY || !amb->route) continue;
        if (amb->routeIndex >= amb->routeLen) continue;

        if (amb->edgeRemaining > 0) amb->edgeRemaining--;
        if (amb->edgeRemaining > 0) continue;

        /* arrived at the next node of the route */
        amb->currentNode = amb->route[amb->routeIndex];
        amb->routeIndex++;

        if (amb->routeIndex >= amb->routeLen) {           /* reached destination */
            if (amb->destinationIsHospital) dispatch_arrivedAtHospital(w, amb);
            else                            dispatch_arrivedAtScene(w, amb);
        } else {
            int wt;                                       /* LIVE weight: traffic-aware */
            if (graph_edgeWeight(&w->graph, amb->currentNode, amb->route[amb->routeIndex], &wt) != 0)
                wt = 1;                                   /* defensive */
            amb->edgeTotal = wt;
            amb->edgeRemaining = wt;
        }
    }
}

/*
 * ONE simulation minute, fixed order:
 *   1. dispatch pending emergencies (priority queue order)
 *   2. re-route check under live road conditions  (BEFORE moving, so an
 *      ambulance never drives into a road that is already blocked)
 *   3. move every ambulance one minute
 */
void simulation_tick(World *w) {
    w->clock++;
    if (w->autoDispatch) {
        while (pq_size(&w->pq) > 0) {
            if (dispatch_processNext(w) != 1) break;      /* no ambulance -> retry next tick */
        }
    }
    dispatch_recalculateRoutes(w);
    moveAmbulances(w);
    
}

void simulation_runFor(World *w, long minutes) {
    for (long i = 0; i < minutes; i++) simulation_tick(w);
}

void simulation_runUntilSettled(World *w, long maxMinutes) {
    while (maxMinutes-- > 0) {
        int active = 0;
        for (ListNode *n = w->ambulances.head; n; n = n->next)
            if (((Ambulance *)n->data)->status == AMB_BUSY) { active = 1; break; }
        if (!active && pq_size(&w->pq) == 0) break;
        simulation_tick(w);
    }
}
