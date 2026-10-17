#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "graph.h"
#include "road.h"
#include "emergency.h"
#include "ambulance.h"
#include "hospital.h"
#include "linked_list.h"

static void amb_dtor(void *data) {
    Ambulance *a = (Ambulance *)data;
    free(a->route);
    free(a);
}

int main(void) {
    Graph g;
    List roads, ambulances, hospitals, emergencies;
    assert(graph_init(&g, 4) == 0);
    list_init(&roads); list_init(&ambulances);
    list_init(&hospitals); list_init(&emergencies);

    graph_addNode(&g, "A", 0, 0, NODE_JUNCTION);
    graph_addNode(&g, "B", 0, 0, NODE_JUNCTION);
    graph_addNode(&g, "H", 0, 0, NODE_HOSPITAL);

    /* ---------- road add + lookups ---------- */
    Road *r = road_add(&roads, &g, 1, 0, 1, 5);
    assert(r && road_findById(&roads, 1) == r);
    assert(road_findByNodes(&roads, 1, 0) == r);          /* reverse lookup */
    int w;
    assert(graph_edgeWeight(&g, 0, 1, &w) == 0 && w == 5);
    assert(graph_edgeWeight(&g, 1, 0, &w) == 0 && w == 5); /* both directions */
    assert(road_add(&roads, &g, 2, 1, 0, 5) == NULL);      /* duplicate rejected */
    assert(road_add(&roads, &g, 3, 0, 0, 5) == NULL);      /* self-loop rejected */
    assert(road_add(&roads, &g, 4, 0, 1, 0) == NULL);      /* bad baseTime */

    /* ---------- direct condition set ---------- */
    assert(road_setCondition(&roads, &g, 1, ROAD_HEAVY) == 0);
    assert(r->currentTime == 15 && r->blocked == 0);
    assert(road_setCondition(&roads, &g, 99, ROAD_NORMAL) == -1);

    /* ---------- traffic cycle: HEAVY -> down -> up, capped at HEAVY ---------- */
    assert(road_decreaseTraffic(&roads, &g, 1) == 0);
    assert(road_conditionOf(r) == ROAD_TRAFFIC);
    assert(graph_edgeWeight(&g, 0, 1, &w) == 0 && w == 10);   /* base*2 */
    assert(road_decreaseTraffic(&roads, &g, 1) == 0);
    assert(road_conditionOf(r) == ROAD_NORMAL);
    assert(road_decreaseTraffic(&roads, &g, 1) == 1);         /* floor: NORMAL */

    assert(road_increaseTraffic(&roads, &g, 1) == 0);
    assert(road_conditionOf(r) == ROAD_TRAFFIC);
    assert(road_increaseTraffic(&roads, &g, 1) == 0);
    assert(road_conditionOf(r) == ROAD_HEAVY);
    assert(graph_edgeWeight(&g, 0, 1, &w) == 0 && w == 15);   /* base*3 */
    assert(road_increaseTraffic(&roads, &g, 1) == 1);         /* cap: HEAVY */
    assert(road_increaseTraffic(&roads, &g, 99) == -1);       /* not found */

    /* ---------- block / open ---------- */
    assert(road_block(&roads, &g, 1) == 0);
    assert(road_conditionOf(r) == ROAD_BLOCKED);
    assert(graph_edgeWeight(&g, 0, 1, &w) == -1);             /* unusable */
    assert(graph_edgeWeight(&g, 1, 0, &w) == -1);             /* BOTH ways */
    assert(road_increaseTraffic(&roads, &g, 1) == 1);         /* no traffic ops */
    assert(road_decreaseTraffic(&roads, &g, 1) == 1);         /* on blocked    */
    assert(road_open(&roads, &g, 1) == 0);
    assert(road_conditionOf(r) == ROAD_NORMAL);
    assert(graph_edgeWeight(&g, 0, 1, &w) == 0 && w == 5);    /* base restored */

    /* ---------- condition counting ---------- */
    road_add(&roads, &g, 2, 1, 2, 3);
    road_block(&roads, &g, 2);
    assert(list_size(&roads) == 2);
    assert(road_countByCondition(&roads, ROAD_BLOCKED) == 1);
    assert(road_countByCondition(&roads, ROAD_NORMAL) == 1);

    /* ---------- ambulance: create, count, route ownership ---------- */
    Ambulance *a1 = ambulance_create(&ambulances, 1, 0, AMB_AVAILABLE, AMB_GENERAL);
    assert(a1 && ambulance_findById(&ambulances, 1) == a1);
    assert(ambulance_findById(&ambulances, 99) == NULL);
    Ambulance *a2 = ambulance_create(&ambulances, 2, 1, AMB_MAINTENANCE, AMB_ICU);
    assert(ambulance_countByStatus(&ambulances, AMB_AVAILABLE) == 1);
    assert(ambulance_countByStatus(&ambulances, AMB_MAINTENANCE) == 1);

    int path[3] = { 0, 1, 2 };
    Route r2; r2.success = 1; r2.totalCost = 8; r2.path = path; r2.pathLength = 3;
    assert(ambulance_startRoute(a1, &g, &r2) == 0);           /* moving */
    assert(a1->routeLen == 3 && a1->route[0] == 0 && a1->routeIndex == 1);
    assert(a1->destinationNode == 2);
    assert(a1->edgeTotal == 5);                               /* live weight 0-1 */
    assert(a1->route != path);                                /* deep COPY, no alias */
    ambulance_clearRoute(a1);
    assert(a1->route == NULL && a1->routeLen == 0 && a1->destinationNode == -1);

    int one[1] = { 1 };
    Route r3; r3.success = 1; r3.totalCost = 0; r3.path = one; r3.pathLength = 1;
    assert(ambulance_startRoute(a2, &g, &r3) == 1);           /* already there */
    ambulance_clearRoute(a2);

    a1->status = AMB_BUSY;
    a1->assignedEmergency = 7;
    ambulance_release(a1);
    assert(a1->status == AMB_AVAILABLE && a1->assignedEmergency == -1);
    assert(a1->route == NULL);

    /* ---------- hospital candidate rules ---------- */
    Hospital *h1 = hospital_create(&hospitals, 1, 2, "CityCare", 3, 1, 1);
    assert(h1 && hospital_findByNode(&hospitals, 2) == h1);
    Emergency *e1 = emergency_create(&emergencies, 1, 0, SEV_HIGH, EMG_NEED_GENERAL, 1, 5);
    Emergency *e2 = emergency_create(&emergencies, 2, 0, SEV_CRITICAL, EMG_NEED_ICU, 2, 7);

    assert(hospital_isCandidate(h1, e1) == 1);      /* 1 general, 3 beds       */
    assert(hospital_isCandidate(h1, e2) == 1);      /* ICU ok, 2 pat, 3 beds   */

    h1->beds = 1;
    assert(hospital_isCandidate(h1, e2) == 0);      /* only 1 bed, 2 patients  */
    assert(hospital_isCandidate(h1, e1) == 1);

    h1->beds = 5; h1->icuAvailable = 0;
    assert(hospital_isCandidate(h1, e2) == 0);      /* ICU needed, unavailable */
    assert(hospital_isCandidate(h1, e1) == 1);

    h1->emergencyService = 0;
    assert(hospital_isCandidate(h1, e1) == 0);      /* no emergency service    */

    /* ---------- emergency defaults ---------- */
    assert(e1->assignedAmbulance == -1 && e1->assignedHospital == -1);
    assert(e1->status == EMG_QUEUED && e1->completedAt == -1);
    assert(e1->patients == 1 && e1->createdAt == 5);
    assert(emergency_findById(&emergencies, 2) == e2);
    assert(emergency_isActive(e1) == 1);
    e1->status = EMG_COMPLETED;
    assert(emergency_isActive(e1) == 0);

    /* ---------- cleanup (valgrind-clean) ---------- */
    graph_free(&g);
    list_clear(&roads, free);
    list_clear(&hospitals, free);
    list_clear(&emergencies, free);
    list_clear(&ambulances, amb_dtor);

    printf("test_managers: ALL PASSED\n");
    return 0;
}
