#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "graph.h"

int main(void) {
    Graph g;
    assert(graph_init(&g, 2) == 0);

    /* ids are sequential */
    assert(graph_addNode(&g, "A", 0, 0, NODE_JUNCTION) == 0);
    assert(graph_addNode(&g, "B", 10, 0, NODE_HOSPITAL) == 1);
    assert(graph_addNode(&g, "C", 20, 0, NODE_STATION) == 2);
    assert(graph_addNode(&g, "D", 30, 0, NODE_JUNCTION) == 3);  /* forces capacity growth 2->4->? */
    assert(g.numNodes == 4);

    assert(graph_getNode(&g, 2) != NULL && strcmp(graph_getNode(&g, 2)->name, "C") == 0);
    assert(graph_getNode(&g, -1) == NULL && graph_getNode(&g, 99) == NULL);

    /* undirected edges; duplicates / self-loops / invalid rejected */
    assert(graph_addEdge(&g, 0, 1, 5) == 0);
    assert(graph_addEdge(&g, 0, 1, 9) == -1);
    assert(graph_addEdge(&g, 0, 0, 3) == -1);
    assert(graph_addEdge(&g, 0, 99, 3) == -1);
    assert(graph_addEdge(&g, 0, 1, -2) == -1);

    Edge *e = graph_findEdge(&g, 0, 1);
    assert(e && e->weight == 5 && e->blocked == 0);
    Edge *rev = graph_findEdge(&g, 1, 0);
    assert(rev && rev->weight == 5);

    /* updates sync BOTH directions */
    assert(graph_setEdgeWeight(&g, 0, 1, 10) == 0);
    assert(graph_findEdge(&g, 1, 0)->weight == 10);
    assert(graph_setEdgeBlocked(&g, 0, 1, 1) == 0);
    assert(graph_findEdge(&g, 1, 0)->blocked == 1);

    int w = 0;
    assert(graph_edgeWeight(&g, 0, 1, &w) == -1);       /* blocked -> unusable */
    assert(graph_setEdgeBlocked(&g, 0, 1, 0) == 0);
    assert(graph_edgeWeight(&g, 0, 1, &w) == 0 && w == 10);

    graph_free(&g);                                      /* valgrind-clean */
    printf("test_graph: ALL PASSED\n");
    return 0;
}
