#ifndef DISPATCH_H
#define DISPATCH_H

#include "simulation.h"

/*
 * DISPATCH = decision engine.
 * Dijkstra only answers "best route A->B?" — WHO goes WHERE is decided here,
 * using the resource managers. This separation is the core architecture rule.
 *
 * Return codes (common): 1 = handled, 0 = nothing to do, -1 = failed cleanly.
 */
int dispatch_processNext(World *w);            /* assign top-priority emergency */
int dispatch_arrivedAtScene(World *w, Ambulance *amb);
int dispatch_arrivedAtHospital(World *w, Ambulance *amb);
int dispatch_recalculateRoutes(World *w);      /* all moving ambulances */
int dispatch_recalculateRoute(World *w, Ambulance *amb);
int dispatch_selectAmbulance(World *w, const Emergency *e, Ambulance **best, int *bestETA);
int dispatch_selectHospital(World *w, const Emergency *e, int fromNode, Hospital **best, int *bestETA);

#endif
