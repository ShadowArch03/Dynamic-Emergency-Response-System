#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hospital.h"

static int matchHospitalId(void *data, void *context) {
    return ((Hospital *)data)->id == *(int *)context;
}

Hospital *hospital_create(List *hospitals, int id, int nodeId, const char *name,
                          int beds, int emergencyService, int icu) {
    Hospital *h = calloc(1, sizeof(Hospital));
    if (!h) return NULL;
    h->id = id;
    h->nodeId = nodeId;
    if (name && name[0]) {
        strncpy(h->name, name, MAX_NAME - 1);   /* bounded copy ... */
        h->name[MAX_NAME - 1] = '\0';           /* ... forced NUL   */
    } else {
        snprintf(h->name, MAX_NAME, "Hospital-%d", id);
    }
    h->beds = beds > 0 ? beds : 0;
    h->totalBeds = h->beds;
    h->emergencyService = emergencyService ? 1 : 0;
    h->icuAvailable = icu ? 1 : 0;
    if (list_push_back(hospitals, h) != 0) { free(h); return NULL; }
    return h;
}

Hospital *hospital_findById(const List *hospitals, int id) {
    return (Hospital *)list_find(hospitals, matchHospitalId, &id);
}

Hospital *hospital_findByNode(const List *hospitals, int nodeId) {
    for (ListNode *n = hospitals->head; n; n = n->next)
        if (((Hospital *)n->data)->nodeId == nodeId) return (Hospital *)n->data;
    return NULL;
}

/*
 * VALID candidate = can actually accept THIS emergency:
 * has emergency service + enough beds for ALL patients + ICU when needed.
 * (Reachability is checked separately via Dijkstra in dispatch.)
 */
int hospital_isCandidate(const Hospital *h, const Emergency *e) {
    if (!h || !e) return 0;
    if (!h->emergencyService) return 0;
    if (h->beds < e->patients) return 0;
    if (e->type == EMG_NEED_ICU && !h->icuAvailable) return 0;
    return 1;
}

void hospital_printAll(const List *hospitals) {
    printf("%-4s %-6s %-26s %-6s %-6s %-5s\n",
           "ID", "NODE", "NAME", "BEDS", "EMERG", "ICU");
    for (ListNode *n = hospitals->head; n; n = n->next) {
        Hospital *h = (Hospital *)n->data;
        printf("H%-3d %-6d %-26s %-6d %-6s %-5s\n",
               h->id, h->nodeId, h->name, h->beds,
               h->emergencyService ? "yes" : "no",
               h->icuAvailable ? "yes" : "no");
    }
}
