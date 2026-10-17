#ifndef MAP_H
#define MAP_H

#include "simulation.h"

/*
 * Terminal map dashboard. READS the same World the algorithms use -
 * never computes routes. (Phase 2: raylib GUI replaces this renderer,
 * engine stays identical.)
 */
void map_render(const World *w);

#endif