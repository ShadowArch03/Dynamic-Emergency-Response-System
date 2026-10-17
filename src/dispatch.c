#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dispatch.h"
#include "emergency.h"
#include "ambulance.h"
#include "hospital.h"
#include "report.h"

/* ============ AMBULANCE SELECTION ============
 * For every AVAILABLE, type-compatible ambulance run Dijkstra to the scene
 * under CURRENT road conditions; pick minimum ETA.
 * Tie-breaker: lower ambulance id (deterministic, testable).            */
int dispatch_selectAmbulance(World *w, const Emergency *e, Ambulance **best, int *bestETA) {
    *best = NULL;
    *bestETA = INF_COST;
    int bestId = INF_COST;

    for (ListNode *n = w->ambulances.head; n; n = n->next) {
        Ambulance *amb = (Ambulance *)n->data;
        if (amb->status != AMB_AVAILABLE) continue;
        if (e->type == EMG_NEED_ICU && amb->type != AMB_ICU) continue;  /* ICU call needs ICU unit */

        Route r = dijkstra_shortestPath(&w->graph, amb->currentNode, e->locationNode);
        if (!r.success) { route_free(&r); continue; }                    /* can't reach scene */

        int better = (r.totalCost < *bestETA) ||
                     (r.totalCost == *bestETA && amb->id < bestId);
        if (better) { *best = amb; *bestETA = r.totalCost; bestId = amb->id; }
        route_free(&r);
    }
    return *best != NULL;
}

/* ============ HOSPITAL SELECTION ============
 * Step 1: VALID candidates only (beds + facility) — hospital_isCandidate().
 * Step 2: reachable from fromNode (Dijkstra).
 * Step 3: minimum ETA; tie-breakers: more free beds, then lower id.
 * NOTE: fromNode is the CURRENT ambulance position — never the old start. */
int dispatch_selectHospital(World *w, const Emergency *e, int fromNode, Hospital **best, int *bestETA) {
    *best = NULL;
    *bestETA = INF_COST;

    for (ListNode *n = w->hospitals.head; n; n = n->next) {
        Hospital *h = (Hospital *)n->data;

        if (!hospital_isCandidate(h, e)) {
            if (!h->emergencyService)
                report_log(w, "Hospital %s skipped: no emergency service", h->name);
            else if (h->beds < e->patients)
                report_log(w, "Hospital %s skipped: not enough beds (%d free, %d needed)",
                           h->name, h->beds, e->patients);
            else if (e->type == EMG_NEED_ICU && !h->icuAvailable)
                report_log(w, "Hospital %s skipped: ICU required but not available", h->name);
            continue;
        }
        Route r = dijkstra_shortestPath(&w->graph, fromNode, h->nodeId);
        if (!r.success) {
            report_log(w, "Hospital %s skipped: unreachable from node %d", h->name, fromNode);
            route_free(&r);
            continue;
        }
        int better = (r.totalCost < *bestETA);
        if (!better && r.totalCost == *bestETA && *best) {
            if (h->beds > (*best)->beds) better = 1;                          /* tie: more beds */
            else if (h->beds == (*best)->beds && h->id < (*best)->id) better = 1; /* tie: lower id */
        }
        if (better) { *best = h; *bestETA = r.totalCost; }
        route_free(&r);
    }
    return *best != NULL;
}

/* ============ DISPATCH ============ */
int dispatch_processNext(World *w) {
    PQItem top;
    if (!pq_peek(&w->pq, &top)) return 0;                       /* queue empty */
    Emergency *e = emergency_findById(&w->emergencies, top.emergencyId);
    if (!e) { pq_pop(&w->pq, &top); return 0; }                 /* stale entry */

    Ambulance *amb = NULL;
    int eta = INF_COST;
    if (!dispatch_selectAmbulance(w, e, &amb, &eta)) {
        if (!e->warnedNoAmbulance) {                            /* log once, not every tick */
            report_log(w, "E%d (%s): NO suitable ambulance available - remains queued",
                       e->id, severity_name(e->severity));
            e->warnedNoAmbulance = 1;
        }
        return -1;                                              /* stays QUEUED, retried next tick */
    }

    pq_pop(&w->pq, &top);                                       /* pop only AFTER success */
    Route r = dijkstra_shortestPath(&w->graph, amb->currentNode, e->locationNode);
    if (!r.success) { route_free(&r); return -1; }              /* defensive */

    amb->status = AMB_BUSY;
    amb->assignedEmergency = e->id;
    amb->destinationIsHospital = 0;
    e->assignedAmbulance = amb->id;
    e->status = EMG_ASSIGNED;
    e->warnedNoAmbulance = 0;
    w->dispatched++;

    char routeStr[128];
    route_toString(r.path, r.pathLength, routeStr, (int)sizeof routeStr);
    report_log(w, "E%d (%s %s) dispatched: ambulance A%d from node %d, ETA %d min, route %s",
               e->id, severity_name(e->severity), emergencyType_name(e->type),
               amb->id, amb->currentNode, r.totalCost, routeStr);

    int arrivedNow = ambulance_startRoute(amb, &w->graph, &r);
    route_free(&r);
    if (arrivedNow == 1) return dispatch_arrivedAtScene(w, amb);  /* ambulance was already there */
    return 1;
}

/* ============ ARRIVED AT SCENE -> hospital selection FROM HERE ============ */
int dispatch_arrivedAtScene(World *w, Ambulance *amb) {
    Emergency *e = emergency_findById(&w->emergencies, amb->assignedEmergency);
    if (!e) { ambulance_release(amb); return -1; }

    Node *scene = graph_getNode(&w->graph, e->locationNode);
    report_log(w, "A%d reached emergency E%d at %s (node %d)",
               amb->id, e->id, scene ? scene->name : "?", e->locationNode);

    Hospital *h = NULL;
    int eta = INF_COST;
    if (!dispatch_selectHospital(w, e, amb->currentNode, &h, &eta)) {
        report_log(w, "E%d: NO SUITABLE HOSPITAL AVAILABLE - response failed", e->id);
        e->status = EMG_FAILED_NO_HOSPITAL;
        e->completedAt = w->clock;
        w->failed++;
        ambulance_release(amb);
        return -1;
    }

    h->beds -= e->patients;                       /* RESERVE bed NOW (no double-booking race) */
    if (h->beds < 0) h->beds = 0;
    e->assignedHospital = h->id;
    e->status = EMG_TRANSPORTING;

    Route r = dijkstra_shortestPath(&w->graph, amb->currentNode, h->nodeId);
    if (!r.success) {                             /* defensive: selection verified reachability */
        route_free(&r);
        h->beds += e->patients;
        if (h->beds > h->totalBeds) h->beds = h->totalBeds;
        e->status = EMG_FAILED_NO_HOSPITAL;
        e->completedAt = w->clock;
        w->failed++;
        ambulance_release(amb);
        return -1;
    }
    amb->destinationIsHospital = 1;
    char routeStr[128];
    route_toString(r.path, r.pathLength, routeStr, (int)sizeof routeStr);
    report_log(w, "E%d: hospital %s selected (beds left %d), ETA %d min, route %s",
               e->id, h->name, h->beds, r.totalCost, routeStr);
    int arrivedNow = ambulance_startRoute(amb, &w->graph, &r);
    route_free(&r);
    if (arrivedNow == 1) return dispatch_arrivedAtHospital(w, amb);
    return 1;
}

/* ============ PATIENT DELIVERED ============ */
int dispatch_arrivedAtHospital(World *w, Ambulance *amb) {
    Emergency *e = emergency_findById(&w->emergencies, amb->assignedEmergency);
    if (!e) { ambulance_release(amb); return -1; }
    Hospital *h = hospital_findById(&w->hospitals, e->assignedHospital);

    e->status = EMG_COMPLETED;
    e->completedAt = w->clock;
    w->completed++;
    report_log(w, "A%d delivered E%d (%d patient%s) to %s - response time %ld min (reroutes: %d)",
               amb->id, e->id, e->patients, e->patients == 1 ? "" : "s",
               h ? h->name : "?", e->completedAt - e->createdAt, e->rerouteCount);

    amb->currentNode = amb->destinationNode;
    ambulance_release(amb);
    return 1;
}

/* ============ DYNAMIC RE-ROUTING (runs every tick for every moving ambulance)
 *
 * 1. Compute the HONEST remaining cost of the CURRENT plan under LIVE weights:
 *    the edge being driven right now uses its FRESH weight minus the minutes
 *    already spent on it (traffic increase slows the ambulance honestly).
 * 2. Fresh Dijkstra from the CURRENT node (never the original start!).
 * 3. Re-route if any edge ahead is blocked OR the fresh route is strictly
 *    cheaper (strict < keeps the plan stable and prevents flip-flopping).
 *    If the new route continues on the same edge, the progress already made
 *    is PRESERVED (edgeRemaining is kept).
 * 4. If we keep the plan but the current edge's weight changed, the timing
 *    fields are re-synced to the live weight.
 * 5. If the destination became unreachable:
 *      - hospital destination -> give the reserved bed back and RE-SELECT a
 *        hospital from the CURRENT node;
 *      - scene destination    -> response fails cleanly.
 */
int dispatch_recalculateRoute(World *w, Ambulance *amb) {
    if (!amb || amb->status != AMB_BUSY || !amb->route) return 0;
    if (amb->routeIndex >= amb->routeLen) return 0;

    Emergency *e = emergency_findById(&w->emergencies, amb->assignedEmergency);
    if (!e) return 0;

    /* --- 1. honest remaining cost of the current plan --- */
    int curFrom = amb->route[amb->routeIndex - 1];      /* == amb->currentNode */
    int curTo   = amb->route[amb->routeIndex];

    int freshCurW = 0;
    int curOk = (graph_edgeWeight(&w->graph, curFrom, curTo, &freshCurW) == 0);

    int spent = amb->edgeTotal - amb->edgeRemaining;    /* minutes consumed on this edge */
    int remCurrent = INF_COST;
    if (curOk) {
        remCurrent = freshCurW - spent;
        if (remCurrent < 0) remCurrent = 0;
    }

    int ahead = route_costFrom(&w->graph, amb->route, amb->routeLen, amb->routeIndex);
    long remainingCurrent = (long)remCurrent + (ahead < 0 ? (long)INF_COST : ahead);

    int blockedAhead = route_hasBlockedEdge(&w->graph, amb->route, amb->routeLen,
                                            amb->routeIndex - 1);

    /* --- 2. fresh best route from CURRENT node --- */
    Route fresh = dijkstra_shortestPath(&w->graph, amb->currentNode, amb->destinationNode);

    if (fresh.success) {
        if (fresh.pathLength < 2) {                     /* defensive: on destination */
            route_free(&fresh);
            if (amb->destinationIsHospital) return dispatch_arrivedAtHospital(w, amb);
            return dispatch_arrivedAtScene(w, amb);
        }

        if (blockedAhead || (long)fresh.totalCost < remainingCurrent) {
            /* --- 3. RE-ROUTE --- */
            int keepRemaining = -1;
            if (curOk && fresh.path[1] == curTo)
                keepRemaining = remCurrent;             /* same edge: keep progress */

            int *newRoute = malloc((size_t)fresh.pathLength * sizeof(int));
            if (newRoute) {
                memcpy(newRoute, fresh.path, (size_t)fresh.pathLength * sizeof(int));
                free(amb->route);
                amb->route = newRoute;
                amb->routeLen = fresh.pathLength;
                amb->routeIndex = 1;
                amb->destinationNode = fresh.path[fresh.pathLength - 1];

                int wt;
                if (graph_edgeWeight(&w->graph, amb->route[0], amb->route[1], &wt) != 0)
                    wt = 1;                             /* defensive */
                amb->edgeTotal = wt;
                amb->edgeRemaining = (keepRemaining >= 0 && keepRemaining <= wt)
                                       ? keepRemaining : wt;

                e->rerouteCount++;
                w->reroutes++;
                char routeStr[128];
                route_toString(amb->route, amb->routeLen, routeStr, (int)sizeof routeStr);
                report_log(w, "E%d RE-ROUTED from node %d: new route %s, remaining ETA %d min",
                           e->id, amb->currentNode, routeStr, fresh.totalCost);
            }
               } else if (curOk && (freshCurW != amb->edgeTotal || remCurrent != amb->edgeRemaining)) {
            /* --- 4. keep plan, sync current-edge timing to LIVE weight --- */
            report_log(w, "E%d: traffic slowed current road - remaining time updated %d -> %d min",
                       e->id, amb->edgeRemaining, remCurrent);
            amb->edgeTotal = freshCurW;
            amb->edgeRemaining = remCurrent;
        }
        route_free(&fresh);
        return 1;
    }

    /* --- 5. destination UNREACHABLE --- */
    if (amb->destinationIsHospital) {
        Hospital *old = hospital_findById(&w->hospitals, e->assignedHospital);
        if (old) {                                      /* give the reserved bed back */
            old->beds += e->patients;
            if (old->beds > old->totalBeds) old->beds = old->totalBeds;
        }
        report_log(w, "E%d: hospital %s UNREACHABLE - re-selecting from CURRENT node %d",
                   e->id, old ? old->name : "?", amb->currentNode);

        Hospital *h = NULL;
        int eta = INF_COST;
        if (!dispatch_selectHospital(w, e, amb->currentNode, &h, &eta)) {
            report_log(w, "E%d: NO SUITABLE HOSPITAL - response failed", e->id);
            e->status = EMG_FAILED_NO_HOSPITAL;
            e->completedAt = w->clock;
            w->failed++;
            ambulance_release(amb);
            return -1;
        }
        h->beds -= e->patients;
        if (h->beds < 0) h->beds = 0;
        e->assignedHospital = h->id;
        e->rerouteCount++;                              /* hospital change = route change */
        w->reroutes++;

        Route r = dijkstra_shortestPath(&w->graph, amb->currentNode, h->nodeId);
        if (!r.success) {                               /* defensive */
            route_free(&r);
            h->beds += e->patients;
            if (h->beds > h->totalBeds) h->beds = h->totalBeds;
            e->status = EMG_FAILED_NO_HOSPITAL;
            e->completedAt = w->clock;
            w->failed++;
            ambulance_release(amb);
            return -1;
        }
        amb->destinationNode = h->nodeId;
        amb->destinationIsHospital = 1;
        char routeStr[128];
        route_toString(r.path, r.pathLength, routeStr, (int)sizeof routeStr);
        report_log(w, "E%d: hospital RE-SELECTED: %s, ETA %d min, route %s",
                   e->id, h->name, r.totalCost, routeStr);
        int arrivedNow = ambulance_startRoute(amb, &w->graph, &r);
        route_free(&r);
        if (arrivedNow == 1) return dispatch_arrivedAtHospital(w, amb);
        return 1;
    }

    report_log(w, "E%d: emergency location unreachable - response failed", e->id);
    e->status = EMG_FAILED_UNREACHABLE;
    e->completedAt = w->clock;
    w->failed++;
    ambulance_release(amb);
    return -1;
}

int dispatch_recalculateRoutes(World *w) {
    int changed = 0;
    for (ListNode *n = w->ambulances.head; n; n = n->next) {
        Ambulance *amb = (Ambulance *)n->data;
        if (amb->status == AMB_BUSY && amb->route && amb->routeIndex < amb->routeLen)
            if (dispatch_recalculateRoute(w, amb) == 1) changed++;
    }
    return changed;
}
