#include <assert.h>
#include <stdio.h>
#include<string.h>
#include "dijkstra.h"

int main(void) {
    Graph g;
    assert(graph_init(&g, 8) == 0);
    for (int i = 0; i < 6; i++) graph_addNode(&g, "N", i, i, NODE_JUNCTION);

    graph_addEdge(&g, 0, 1, 7);
    graph_addEdge(&g, 0, 2, 9);
    graph_addEdge(&g, 1, 2, 10);
    graph_addEdge(&g, 1, 3, 15);
    graph_addEdge(&g, 2, 3, 11);
    graph_addEdge(&g, 0, 5, 14);
    graph_addEdge(&g, 2, 5, 2);
    graph_addEdge(&g, 3, 4, 6);
    graph_addEdge(&g, 4, 5, 9);

    /* shortest 0 -> 5 is 0-2-5 with cost 11 */
    Route r = dijkstra_shortestPath(&g, 0, 5);
    assert(r.success && r.totalCost == 11 && r.pathLength == 3);
    assert(r.path[0] == 0 && r.path[1] == 2 && r.path[2] == 5);
    route_free(&r);

    /* shortest 0 -> 3 is 0-2-3 with cost 20 */
    r = dijkstra_shortestPath(&g, 0, 3);
    assert(r.success && r.totalCost == 20 && r.pathLength == 3);
    assert(r.path[0] == 0 && r.path[1] == 2 && r.path[2] == 3);
    route_free(&r);

    /* source == destination */
    r = dijkstra_shortestPath(&g, 3, 3);
    assert(r.success && r.totalCost == 0 && r.pathLength == 1);
    route_free(&r);

    /* blocking 2-5 forces the direct edge (14) */
    graph_setEdgeBlocked(&g, 2, 5, 1);
    r = dijkstra_shortestPath(&g, 0, 5);
    assert(r.success && r.totalCost == 14 && r.pathLength == 2);
    route_free(&r);

    /* blocking everything into 5 -> UNREACHABLE */
    graph_setEdgeBlocked(&g, 0, 5, 1);
    graph_setEdgeBlocked(&g, 4, 5, 1);
    r = dijkstra_shortestPath(&g, 0, 5);
    assert(!r.success && r.path == NULL);
    route_free(&r);

    /* invalid inputs -> safe empty route */
    r = dijkstra_shortestPath(&g, -1, 2);
    assert(!r.success);
    r = dijkstra_shortestPath(&g, 0, 99);
    assert(!r.success);

    /* route helpers (used later by re-routing) */
    graph_setEdgeBlocked(&g, 2, 5, 0);
    graph_setEdgeBlocked(&g, 0, 5, 0);
    graph_setEdgeBlocked(&g, 4, 5, 0);
    int path[3] = { 0, 2, 3 };
    assert(route_costFrom(&g, path, 3, 0) == 9 + 11);
    assert(route_costFrom(&g, path, 3, 1) == 11);
    assert(route_hasBlockedEdge(&g, path, 3, 0) == 0);
    graph_setEdgeBlocked(&g, 2, 3, 1);
    assert(route_hasBlockedEdge(&g, path, 3, 0) == 1);
    assert(route_costFrom(&g, path, 3, 0) == -1);

    char buf[32];
    route_toString(path, 3, buf, (int)sizeof buf);
    assert(strcmp(buf, "0->2->3") == 0);

    graph_free(&g);
    printf("test_dijkstra: ALL PASSED\n");
    return 0;
}

