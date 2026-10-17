#include <assert.h>
#include <stdio.h>
#include "simulation.h"
#include "dispatch.h"
#include "road.h"
#include "ambulance.h"
#include "hospital.h"
#include "emergency.h"

/*
 * Test city:
 *   5 (station) --2-- 0 --3-- 1 --4-- 2 --2-- 3 (H1)
 *                    |              |
 *                   10             5
 *                    |              |
 *                    2              4 (H2)
 */
static void buildWorld(World *w) {
    assert(world_init(w) == 0);
    w->verbose = 0;                              /* silent: assertions do the talking */
    graph_addNode(&w->graph, "A", 0, 0, NODE_JUNCTION);
    graph_addNode(&w->graph, "B", 0, 0, NODE_JUNCTION);
    graph_addNode(&w->graph, "C", 0, 0, NODE_JUNCTION);
    graph_addNode(&w->graph, "H1", 0, 0, NODE_HOSPITAL);
    graph_addNode(&w->graph, "H2", 0, 0, NODE_HOSPITAL);
    graph_addNode(&w->graph, "ST", 0, 0, NODE_STATION);

    road_add(&w->roads, &w->graph, 0, 5, 0, 2);  /* station-A : 2  */
    road_add(&w->roads, &w->graph, 1, 0, 1, 3);  /* A-B       : 3  */
    road_add(&w->roads, &w->graph, 2, 1, 2, 4);  /* B-C       : 4  */
    road_add(&w->roads, &w->graph, 3, 0, 2, 10); /* A-C       : 10 */
    road_add(&w->roads, &w->graph, 4, 2, 3, 2);  /* C-H1      : 2  */
    road_add(&w->roads, &w->graph, 5, 1, 4, 5);  /* B-H2      : 5  */

    ambulance_create(&w->ambulances, 1, 5, AMB_AVAILABLE, AMB_GENERAL);
    hospital_create(&w->hospitals, 1, 3, "H1", 2, 1, 1);
    hospital_create(&w->hospitals, 2, 4, "H2", 2, 1, 0);
}

int main(void) {
    /* ---- 1. dispatch, best route, mid-journey block -> re-route, completion ---- */
    {
        World w; buildWorld(&w);
        Emergency *e = world_createEmergency(&w, 2, SEV_HIGH, EMG_NEED_GENERAL, 1);
        simulation_runFor(&w, 1);

        Ambulance *a1 = ambulance_findById(&w.ambulances, 1);
        assert(a1->status == AMB_BUSY);
        assert(e->assignedAmbulance == 1);
        /* best route is 5->0->1->2 (9), not 5->0->2 (12) */
        assert(a1->routeLen == 4);
        assert(a1->route[0] == 5 && a1->route[1] == 0 && a1->route[2] == 1 && a1->route[3] == 2);

        /* block B-C while the ambulance is en route */
        road_block(&w.roads, &w.graph, 2);
        simulation_runFor(&w, 1);
        assert(e->rerouteCount >= 1);            /* re-routed from CURRENT position */

        simulation_runUntilSettled(&w, 60);
        assert(e->status == EMG_COMPLETED);
        assert(e->assignedHospital == 1);        /* H1: 2 min from scene */
        Hospital *h1 = hospital_findById(&w.hospitals, 1);
        assert(h1->beds == 1);                   /* bed was reserved */
        assert(a1->status == AMB_AVAILABLE && a1->currentNode == 3);
        world_free(&w);
        printf("dispatch: normal response + re-route + completion OK\n");
    }

    /* ---- 2. nearest hospital full -> second hospital chosen ---- */
    {
        World w; buildWorld(&w);
        hospital_findById(&w.hospitals, 1)->beds = 0;
        Emergency *e = world_createEmergency(&w, 2, SEV_MEDIUM, EMG_NEED_GENERAL, 1);
        simulation_runUntilSettled(&w, 60);
        assert(e->status == EMG_COMPLETED);
        assert(e->assignedHospital == 2);        /* forced to H2 */
        world_free(&w);
        printf("dispatch: full hospital fallback OK\n");
    }

    /* ---- 3. hospital unreachable at selection time ---- */
    {
        World w; buildWorld(&w);
        road_block(&w.roads, &w.graph, 4);       /* C-H1 blocked */
        Emergency *e = world_createEmergency(&w, 2, SEV_HIGH, EMG_NEED_GENERAL, 1);
        simulation_runUntilSettled(&w, 80);
        assert(e->status == EMG_COMPLETED);
        assert(e->assignedHospital == 2);        /* H1 unreachable -> H2 */
        world_free(&w);
        printf("dispatch: unreachable hospital fallback OK\n");
    }

    /* ---- 4. no hospital at all -> explicit clean failure ---- */
    {
        World w; buildWorld(&w);
        hospital_findById(&w.hospitals, 1)->beds = 0;
        hospital_findById(&w.hospitals, 2)->beds = 0;
        Emergency *e = world_createEmergency(&w, 2, SEV_HIGH, EMG_NEED_GENERAL, 1);
        simulation_runUntilSettled(&w, 60);
        assert(e->status == EMG_FAILED_NO_HOSPITAL);
        assert(ambulance_findById(&w.ambulances, 1)->status == AMB_AVAILABLE);
        world_free(&w);
        printf("dispatch: no-hospital failure handling OK\n");
    }

    /* ---- 5. priority ordering with a single ambulance ---- */
    {
        World w; buildWorld(&w);
        Emergency *elow  = world_createEmergency(&w, 2, SEV_LOW, EMG_NEED_GENERAL, 1);
        Emergency *ecrit = world_createEmergency(&w, 1, SEV_CRITICAL, EMG_NEED_GENERAL, 1);
        simulation_runFor(&w, 3);
        assert(ecrit->assignedAmbulance == 1);   /* critical gets the only unit */
        assert(elow->assignedAmbulance == -1 && elow->status == EMG_QUEUED);
        world_free(&w);
        printf("dispatch: priority ordering OK\n");
    }

    /* ---- 6. ICU emergency needs an ICU ambulance ---- */
    {
        World w; buildWorld(&w);
        Emergency *e = world_createEmergency(&w, 2, SEV_CRITICAL, EMG_NEED_ICU, 1);
        simulation_runFor(&w, 3);
        assert(e->status == EMG_QUEUED);         /* only a GENERAL unit exists */
        world_free(&w);
        printf("dispatch: ambulance type matching OK\n");
    }

    /* ---- 7. scene unreachable -> stays queued, no crash ---- */
    {
        World w; buildWorld(&w);
        road_block(&w.roads, &w.graph, 1);       /* A-B */
        road_block(&w.roads, &w.graph, 3);       /* A-C */
        Emergency *e = world_createEmergency(&w, 2, SEV_HIGH, EMG_NEED_GENERAL, 1);
        simulation_runFor(&w, 3);
        assert(e->status == EMG_QUEUED);
        world_free(&w);
        printf("dispatch: unreachable scene handling OK\n");
    }

    printf("test_dispatch: ALL PASSED\n");
    return 0;
}

