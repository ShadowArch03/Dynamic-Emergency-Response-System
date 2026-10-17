#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ambulance.h"

static int matchAmbulanceId(void *data, void *context) {
    return ((Ambulance *)data)->id == *(int *)context;
}

Ambulance *ambulance_create(List *ambulances, int id, int currentNode,
                            AmbulanceStatus status, AmbulanceType type) {
    Ambulance *a = calloc(1, sizeof(Ambulance));
    if (!a) return NULL;
    a->id = id;
    a->currentNode = currentNode;
    a->status = status;
    a->type = type;
    a->assignedEmergency = -1;
    a->destinationNode = -1;
    if (list_push_back(ambulances, a) != 0) { free(a); return NULL; }
    return a;
}

Ambulance *ambulance_findById(const List *ambulances, int id) {
    return (Ambulance *)list_find(ambulances, matchAmbulanceId, &id);
}

int ambulance_countByStatus(const List *ambulances, AmbulanceStatus status) {
    int count = 0;
    for (ListNode *n = ambulances->head; n; n = n->next)
        if (((Ambulance *)n->data)->status == status) count++;
    return count;
}

void ambulance_clearRoute(Ambulance *a) {
    free(a->route);
    a->route = NULL;
    a->routeLen = 0;
    a->routeIndex = 0;
    a->edgeRemaining = 0;
    a->edgeTotal = 0;
    a->destinationNode = -1;
}

void ambulance_release(Ambulance *a) {
    ambulance_clearRoute(a);
    a->status = AMB_AVAILABLE;
    a->assignedEmergency = -1;
    a->destinationIsHospital = 0;
}

/*
 * The ambulance takes ownership of a COPY of the route.
 * The caller's Route is temporary (freed right after), so aliasing
 * it would be a dangling-pointer bug - we deep-copy instead.
 * Returns: 1 = already at destination, 0 = moving, -1 = invalid input.
 */
int ambulance_startRoute(Ambulance *a, const Graph *g, const Route *r) {
    if (!a || !r || !r->success || r->pathLength < 1) return -1;

    int *copy = malloc((size_t)r->pathLength * sizeof(int));
    if (!copy) return -1;
    memcpy(copy, r->path, (size_t)r->pathLength * sizeof(int));

    free(a->route);                       /* discard any previous route */
    a->route = copy;
    a->routeLen = r->pathLength;
    a->destinationNode = r->path[r->pathLength - 1];
    a->routeIndex = 1;

    if (r->pathLength == 1) {             /* standing on the target already */
        a->edgeRemaining = 0;
        a->edgeTotal = 0;
        return 1;
    }
    int w;
    if (graph_edgeWeight(g, a->route[0], a->route[1], &w) != 0)
        w = 1;                            /* defensive: edge vanished between
                                              computation and start */
    a->edgeTotal = w;
    a->edgeRemaining = w;
    return 0;
}

void ambulance_printAll(const List *ambulances) {
    printf("%-4s %-7s %-12s %-8s %-6s %-6s %s\n",
           "ID", "NODE", "STATUS", "TYPE", "EMERG", "DEST", "POSITION");
    for (ListNode *n = ambulances->head; n; n = n->next) {
        Ambulance *a = (Ambulance *)n->data;
        char emerg[16] = "-";
        char dest[16] = "-";
        char pos[24] = "idle";
        if (a->assignedEmergency >= 0) snprintf(emerg, sizeof emerg, "E%d", a->assignedEmergency);
        if (a->destinationNode >= 0)    snprintf(dest, sizeof dest, "n%d", a->destinationNode);
        if (a->status == AMB_BUSY && a->route && a->routeLen > 1)
            snprintf(pos, sizeof pos, "%d/%d (%dmin)", a->routeIndex, a->routeLen - 1, a->edgeRemaining);
        printf("A%-3d %-7d %-12s %-8s %-6s %-6s %s\n",
               a->id, a->currentNode, ambulanceStatus_name(a->status),
               ambulanceType_name(a->type), emerg, dest, pos);
    }
}

const char *ambulanceStatus_name(AmbulanceStatus s) {
    switch (s) {
        case AMB_BUSY:        return "BUSY";
        case AMB_MAINTENANCE: return "MAINTENANCE";
        default:              return "AVAILABLE";
    }
}

const char *ambulanceType_name(AmbulanceType t) {
    return t == AMB_ICU ? "ICU" : "GENERAL";
}
