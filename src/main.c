#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "simulation.h"
#include "dispatch.h"
#include "emergency.h"
#include "ambulance.h"
#include "hospital.h"
#include "road.h"
#include "report.h"
#include "map.h"
#include "auth.h"

/* ------------------- forward declarations ------------------- */
static void runDemos(int pause);

/* ------------------- input helpers ------------------- */

static void flushLine(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

static int promptInt(const char *msg, int lo, int hi) {
    int v, ok;
    for (;;) {
        printf("%s [%d-%d]: ", msg, lo, hi);
        ok = scanf("%d", &v);
        flushLine();
        if (ok == EOF) { printf("\n[input closed - exiting]\n"); exit(0); }
        if (ok == 1 && v >= lo && v <= hi) return v;
        printf("  invalid input, try again\n");
    }
}

static void waitEnter(void) {
    printf("\nPress ENTER to continue...");
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

/* Instant dispatch like a real control room (movement needs time ticks). */
static void dispatchPending(World *w) {
    while (pq_size(&w->pq) > 0)
        if (dispatch_processNext(w) != 1) break;
}

static void listNodes(const World *w) {
    printf("City nodes:\n");
    for (int i = 0; i < w->graph.numNodes; i++)
        printf("  %2d  %-24s %s\n", i, w->graph.nodes[i].name,
               nodeType_name(w->graph.nodes[i].type));
}

/* ------------------- shared actions ------------------- */

static void actionCreateEmergency(World *w) {
    listNodes(w);
    int node = promptInt("Emergency location node", 0, w->graph.numNodes - 1);
    int sev  = promptInt("Severity (0=LOW 1=MEDIUM 2=HIGH 3=CRITICAL)", 0, 3);
    int ty   = promptInt("Type (0=GENERAL 1=ICU)", 0, 1);
    int pat  = promptInt("Patients", 1, 10);
    Emergency *e = world_createEmergency(w, node, (Severity)sev, (EmergencyType)ty, pat);
    if (!e) { printf("Failed to create emergency.\n"); return; }
    dispatchPending(w);
    printf("(advance time from the menu to move ambulances)\n");
}

static void actionAdvanceTime(World *w) {
    int mins = promptInt("Advance how many minutes", 1, 240);
    simulation_runFor(w, mins);
}

/* what: 0=block 1=open 2=traffic+ 3=traffic- */
static void actionRoadChange(World *w, int what) {
    road_printAll(&w->roads);
    int id = promptInt("Road id", 0, 999);
    int rc = -1;
    switch (what) {
        case 0: rc = road_block(&w->roads, &w->graph, id);            break;
        case 1: rc = road_open(&w->roads, &w->graph, id);             break;
        case 2: rc = road_increaseTraffic(&w->roads, &w->graph, id);  break;
        case 3: rc = road_decreaseTraffic(&w->roads, &w->graph, id);  break;
    }
    if (rc == 0) {
        Road *r = road_findById(&w->roads, id);
        printf("Road %d -> %s\n", id, road_conditionName(road_conditionOf(r)));
        dispatch_recalculateRoutes(w);   /* live re-route, like a real system */
    } else if (rc == 1) {
        printf("No change possible (already at limit).\n");
    } else {
        printf("Road not found.\n");
    }
}

/* ------------------- driver view ------------------- */

static void showAmbulanceStatus(const World *w, int ambId) {
    Ambulance *a = ambulance_findById(&w->ambulances, ambId);
    if (!a) { printf("Ambulance A%d not found.\n", ambId); return; }
    Node *at = graph_getNode(&w->graph, a->currentNode);
    printf("Ambulance A%d (%s): %s\n", a->id, ambulanceType_name(a->type),
           ambulanceStatus_name(a->status));
    if (a->status == AMB_BUSY && a->route) {
        char rs[160];
        route_toString(a->route, a->routeLen, rs, (int)sizeof rs);
        int ahead = route_costFrom(&w->graph, a->route, a->routeLen, a->routeIndex);
        int eta = a->edgeRemaining + (ahead < 0 ? 0 : ahead);
        Node *dest = graph_getNode(&w->graph, a->destinationNode);
        printf("  Assigned: E%d | Destination: %s (node %d)\n",
               a->assignedEmergency, dest ? dest->name : "?", a->destinationNode);
        printf("  Route: %s\n", rs);
        printf("  Remaining ETA: %d min\n", eta);
    } else {
        printf("  Currently at: %s (node %d)\n", at ? at->name : "?", a->currentNode);
    }
}

/* ------------------- role loops ------------------- */

static void adminLoop(World *w) {
    for (;;) {
        map_render(w);
        printf("\n--- ADMIN MENU (t=%ld) ---\n", w->clock);
        printf(" 1 Refresh map         2 Show ambulances\n");
        printf(" 3 Show hospitals      4 Show roads\n");
        printf(" 5 Show emergencies    6 Create emergency\n");
        printf(" 7 Advance time        8 Block road\n");
        printf(" 9 Open road          10 Traffic + on road\n");
        printf("11 Traffic - on road  12 Summary + log\n");
        printf("13 Run demo scenarios\n");
        printf(" 0 Logout\n");
        int c = promptInt("Choose", 0, 13);
        switch (c) {
            case 0:  return;
            case 1:  break;
            case 2:  ambulance_printAll(&w->ambulances); break;
            case 3:  hospital_printAll(&w->hospitals);   break;
            case 4:  road_printAll(&w->roads);           break;
            case 5:  emergency_printAll(&w->emergencies);break;
            case 6:  actionCreateEmergency(w); break;
            case 7:  actionAdvanceTime(w);    break;
            case 8:  actionRoadChange(w, 0);  break;
            case 9:  actionRoadChange(w, 1);  break;
            case 10: actionRoadChange(w, 2);  break;
            case 11: actionRoadChange(w, 3);  break;
            case 12: report_printSummary(w); report_printLog(w); break;
            case 13: runDemos(1); break;
        }
        if (c != 0) waitEnter();
    }
}

static void citizenLoop(World *w) {
    for (;;) {
        map_render(w);
        printf("\n--- CITIZEN MENU ---\n");
        printf(" 1 Refresh map         2 Report emergency\n");
        printf(" 3 Track emergencies   4 Show hospitals\n");
        printf(" 5 Show ambulances\n");
        printf(" 0 Logout\n");
        int c = promptInt("Choose", 0, 5);
        switch (c) {
            case 0:  return;
            case 1:  break;
            case 2:  actionCreateEmergency(w);           break;
            case 3:  emergency_printAll(&w->emergencies);break;
            case 4:  hospital_printAll(&w->hospitals);   break;
            case 5:  ambulance_printAll(&w->ambulances); break;
        }
        if (c != 0) waitEnter();
    }
}

static void driverLoop(World *w, int ambId) {
    for (;;) {
        map_render(w);
        printf("\n--- DRIVER MENU (ambulance A%d) ---\n", ambId);
        printf(" 1 Refresh map          2 Road conditions\n");
        printf(" 3 Active emergencies   4 My route & ETA\n");
        printf(" 5 Show hospitals\n");
        printf(" 0 Logout\n");
        int c = promptInt("Choose", 0, 5);
        switch (c) {
            case 0:  return;
            case 1:  break;
            case 2:  road_printAll(&w->roads);           break;
            case 3:  emergency_printAll(&w->emergencies);break;
            case 4:  showAmbulanceStatus(w, ambId);      break;
            case 5:  hospital_printAll(&w->hospitals);   break;
        }
        if (c != 0) waitEnter();
    }
}

/* ------------------- demo scenarios ------------------- */

static void demoWorld(World *w, const char *title, const char *story) {
    printf("\n============================================================\n");
    printf("  %s\n", title);
    printf("============================================================\n");
    printf("%s\n\n", story);
    if (world_init(w) != 0) { fprintf(stderr, "world_init failed\n"); exit(1); }
    if (world_loadData(w, NODES_CSV, ROADS_CSV, AMBULANCES_CSV, HOSPITALS_CSV) != 0) {
        fprintf(stderr, "failed to load city data - run from project root\n");
        exit(1);
    }
    w->verbose = 1;
}

static void demoEnd(World *w, int pause) {
    report_printSummary(w);
    world_free(w);
    if (pause) waitEnter();
}

/* Ticks until the ambulance reaches a given node (mid-journey hook). */
static void runUntilAmbulanceAt(World *w, int ambId, int node, long maxTicks) {
    for (long i = 0; i < maxTicks; i++) {
        Ambulance *a = ambulance_findById(&w->ambulances, ambId);
        if (a && a->currentNode == node) return;
        simulation_tick(w);
    }
}

static void demo1(int pause) {
    World w;
    demoWorld(&w, "SCENARIO 1: Normal emergency response",
        "High-severity accident at Central Junction (node 0).\n"
        "Full pipeline: priority queue -> best ambulance by ETA (Dijkstra)\n"
        "-> scene -> best VALID hospital -> delivery.");
    world_createEmergency(&w, 0, SEV_HIGH, EMG_NEED_GENERAL, 1);
    simulation_runUntilSettled(&w, 60);
    demoEnd(&w, pause);
}

static void demo2(int pause) {
    World w;
    demoWorld(&w, "SCENARIO 2: Priority queue ordering",
        "Three emergencies created in order: LOW, CRITICAL, HIGH.\n"
        "The binary heap must dispatch CRITICAL first, then HIGH, then LOW.");
    world_createEmergency(&w, 4, SEV_LOW,      EMG_NEED_GENERAL, 1);
    world_createEmergency(&w, 8, SEV_CRITICAL, EMG_NEED_GENERAL, 1);
    world_createEmergency(&w, 2, SEV_HIGH,     EMG_NEED_GENERAL, 1);
    simulation_runUntilSettled(&w, 120);
    demoEnd(&w, pause);
}

static void demo3(int pause) {
    World w;
    demoWorld(&w, "SCENARIO 3: Road blockage -> re-route from CURRENT location",
        "Accident at Central Junction. A1 takes route 12->4->0.\n"
        "After A1 reaches node 4, road 5 (4-0) is blocked by debris.\n"
        "The system re-routes FROM NODE 4 - not from the original start.");
    world_createEmergency(&w, 0, SEV_HIGH, EMG_NEED_GENERAL, 1);
    simulation_runFor(&w, 2);                       /* A1 now at node 4 */
    printf("\n>> debris BLOCKS road 5 (4-0) - the road A1 is about to use\n\n");
    road_block(&w.roads, &w.graph, 5);
    dispatch_recalculateRoutes(&w);
    simulation_runUntilSettled(&w, 80);
    demoEnd(&w, pause);
}

static void demo4(int pause) {
    World w;
    demoWorld(&w, "SCENARIO 4: Traffic increase -> cheaper alternate route",
        "Accident at Central Junction. A1 takes route 12->4->0 (ETA 6).\n"
        "At node 4, traffic on road 5 (4-0) rises: first to TRAFFIC (8 min),\n"
        "then HEAVY (12 min). At HEAVY the alternate route (11 min) becomes\n"
        "cheaper - the system must re-route BY COST, not just by blockage.");
    world_createEmergency(&w, 0, SEV_HIGH, EMG_NEED_GENERAL, 1);
    simulation_runFor(&w, 2);                       /* A1 now at node 4 */
    printf("\n>> traffic on road 5 rises to TRAFFIC (4 -> 8 min): still cheapest, A1 slows down honestly\n\n");
    road_increaseTraffic(&w.roads, &w.graph, 5);
    dispatch_recalculateRoutes(&w);
    printf("\n>> traffic on road 5 rises to HEAVY (8 -> 12 min): alternate (11 min) is now cheaper\n\n");
    road_increaseTraffic(&w.roads, &w.graph, 5);
    dispatch_recalculateRoutes(&w);
    simulation_runUntilSettled(&w, 80);
    demoEnd(&w, pause);
}

static void demo5(int pause) {
    World w;
    demoWorld(&w, "SCENARIO 5: Nearest hospital full -> valid-candidate fallback",
        "Accident at South Junction (node 2). The nearest hospital\n"
        "(South Central, 3 min away) reports ZERO free beds. The system\n"
        "must skip it and choose the next best VALID hospital by ETA.");
    hospital_findById(&w.hospitals, 3)->beds = 0;
    report_log(&w, "South Central Hospital reports ZERO free beds");
    world_createEmergency(&w, 2, SEV_HIGH, EMG_NEED_GENERAL, 1);
    simulation_runUntilSettled(&w, 100);
    demoEnd(&w, pause);
}

static void demo6(int pause) {
    World w;
    demoWorld(&w, "SCENARIO 6: Hospital unreachable mid-journey ->\n   re-selection from CURRENT ambulance location",
        "Accident at Central Junction. South Central Hospital is selected\n"
        "(route 0->2->11). When the ambulance is ALREADY at node 2, BOTH\n"
        "roads into that hospital get blocked. Re-selection must run from\n"
        "node 2 (current position), NOT from the accident scene (node 0).");
    world_createEmergency(&w, 0, SEV_HIGH, EMG_NEED_GENERAL, 1);
    runUntilAmbulanceAt(&w, 1, 2, 40);              /* A1 reaches node 2 */
    printf("\n>> both roads into South Central (20: 2-11, 21: 8-11) BLOCKED\n\n");
    road_block(&w.roads, &w.graph, 20);
    road_block(&w.roads, &w.graph, 21);
    dispatch_recalculateRoutes(&w);
    simulation_runUntilSettled(&w, 100);
    demoEnd(&w, pause);
}

static void demo7(int pause) {
    World w;
    demoWorld(&w, "SCENARIO 7: No hospital available -> clean failure",
        "Every hospital reports zero beds. The system must clearly FAIL\n"
        "this emergency - it must never silently choose a wrong hospital.\n"
        "The ambulance is released back to service.");
    for (ListNode *n = w.hospitals.head; n; n = n->next)
        ((Hospital *)n->data)->beds = 0;
    report_log(&w, "ALL hospitals report ZERO available beds");
    world_createEmergency(&w, 0, SEV_CRITICAL, EMG_NEED_GENERAL, 1);
    simulation_runUntilSettled(&w, 80);
    demoEnd(&w, pause);
}

static void runDemos(int pause) {
    demo1(pause); demo2(pause); demo3(pause); demo4(pause);
    demo5(pause); demo6(pause); demo7(pause);
    printf("\nAll 7 demo scenarios finished.\n");
}

/* ------------------- interactive app ------------------- */

static void interactive(void) {
    World w;
    printf("*********************************************************\n");
    printf("*   DYNAMIC EMERGENCY RESPONSE & ROUTE OPTIMIZATION    *\n");
    printf("*   DSA Semester Project (C99) - terminal build         *\n");
    printf("*********************************************************\n");

    if (world_init(&w) != 0) return;
    if (world_loadData(&w, NODES_CSV, ROADS_CSV, AMBULANCES_CSV, HOSPITALS_CSV) != 0) {
        fprintf(stderr, "Failed to load data files - run from the PROJECT ROOT.\n");
        world_free(&w);
        return;
    }
    int loaded = world_loadEmergencies(&w, EMERGENCIES_CSV);
    if (loaded > 0)
        printf("Loaded %d pending emergency(ies) from %s\n", loaded, EMERGENCIES_CSV);
    printf("Tip: login as ADMIN -> option 13 runs all demo scenarios.\n");

    for (;;) {
        Role role = auth_login();
        if (role == ROLE_EXIT) break;
        if (role == ROLE_NONE) continue;

        if (role == ROLE_ADMIN) {
            adminLoop(&w);
        } else if (role == ROLE_CITIZEN) {
            citizenLoop(&w);
        } else {
            ambulance_printAll(&w.ambulances);
            int id = promptInt("Your ambulance id", 1, 99);
            if (!ambulance_findById(&w.ambulances, id))
                printf("No such ambulance.\n");
            else
                driverLoop(&w, id);
        }
    }

    if (report_save(&w, REPORT_FILE) == 0)
        printf("Report saved to %s\n", REPORT_FILE);
    world_free(&w);
    printf("Goodbye.\n");
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        runDemos(0);
        return 0;
    }
    interactive();
    return 0;
}