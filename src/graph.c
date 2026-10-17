#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"

/* Grow nodes[] and adj[] when full. Returns 0 on success. */
static int graph_ensureCapacity(Graph *g, int needed) {
    if (needed <= g->capacity) return 0;
    int newCap = g->capacity ? g->capacity * 2 : 16;
    while (newCap < needed) newCap *= 2;

    Node *nn = realloc(g->nodes, (size_t)newCap * sizeof(Node));
    if (!nn) return -1;
    g->nodes = nn;

    Edge **na = realloc(g->adj, (size_t)newCap * sizeof(Edge *));
    if (!na) return -1;
    g->adj = na;
    for (int i = g->capacity; i < newCap; i++) g->adj[i] = NULL;
    g->capacity = newCap;
    return 0;
}

int graph_init(Graph *g, int capacity) {
    if (capacity <= 0) capacity = 16;
    g->nodes = calloc((size_t)capacity, sizeof(Node));
    g->adj   = calloc((size_t)capacity, sizeof(Edge *));
    if (!g->nodes || !g->adj) {          /* clean up half-initialized state */
        free(g->nodes); free(g->adj);
        g->nodes = NULL; g->adj = NULL;
        return -1;
    }
    g->numNodes = 0;
    g->capacity = capacity;
    return 0;
}

void graph_free(Graph *g) {
    if (!g) return;
    for (int i = 0; i < g->numNodes; i++) {   /* free every adjacency edge */
        Edge *e = g->adj[i];
        while (e) { Edge *next = e->next; free(e); e = next; }
    }
    free(g->adj);
    free(g->nodes);
    g->adj = NULL; g->nodes = NULL;
    g->numNodes = 0; g->capacity = 0;
}

int graph_addNode(Graph *g, const char *name, int x, int y, NodeType type) {
    if (!name) return -1;
    if (graph_ensureCapacity(g, g->numNodes + 1) != 0) return -1;
    Node *n = &g->nodes[g->numNodes];
    n->id = g->numNodes;                  /* id == index, always */
    strncpy(n->name, name, MAX_NAME - 1); /* bounded copy ...     */
    n->name[MAX_NAME - 1] = '\0';         /* ... forced NUL      */
    n->x = x; n->y = y; n->type = type;
    g->adj[g->numNodes] = NULL;
    return g->numNodes++;                 /* returns the new id */
}

Node *graph_getNode(const Graph *g, int id) {
    if (id < 0 || id >= g->numNodes) return NULL;   /* bounds check */
    return &g->nodes[id];
}

Edge *graph_findEdge(const Graph *g, int from, int to) {
    if (from < 0 || from >= g->numNodes || to < 0 || to >= g->numNodes) return NULL;
    for (Edge *e = g->adj[from]; e; e = e->next)
        if (e->destination == to) return e;
    return NULL;
}

int graph_addEdge(Graph *g, int from, int to, int weight) {
    if (from < 0 || from >= g->numNodes || to < 0 || to >= g->numNodes) return -1;
    if (from == to || weight < 0) return -1;              /* invalid edge  */
    if (graph_findEdge(g, from, to)) return -1;           /* duplicate    */

    Edge *a = malloc(sizeof(Edge));
    Edge *b = malloc(sizeof(Edge));
    if (!a || !b) { free(a); free(b); return -1; }
    a->destination = to;   a->weight = weight; a->blocked = 0;
    b->destination = from; b->weight = weight; b->blocked = 0;
    a->next = g->adj[from]; g->adj[from] = a;   /* insert at head: O(1) */
    b->next = g->adj[to];   g->adj[to]   = b;
    return 0;
}

int graph_setEdgeWeight(Graph *g, int from, int to, int weight) {
    Edge *a = graph_findEdge(g, from, to);
    if (!a) return -1;
    a->weight = weight;
    Edge *b = graph_findEdge(g, to, from);      /* keep both directions in sync */
    if (b) b->weight = weight;
    return 0;
}

int graph_setEdgeBlocked(Graph *g, int from, int to, int blocked) {
    Edge *a = graph_findEdge(g, from, to);
    if (!a) return -1;
    a->blocked = blocked;
    Edge *b = graph_findEdge(g, to, from);
    if (b) b->blocked = blocked;
    return 0;
}

/* Returns 0 and the weight if the edge exists AND is not blocked, else -1. */
int graph_edgeWeight(const Graph *g, int from, int to, int *weight) {
    Edge *e = graph_findEdge(g, from, to);
    if (!e || e->blocked) return -1;
    if (weight) *weight = e->weight;
    return 0;
}

void graph_print(const Graph *g) {
    printf("City graph: %d nodes\n", g->numNodes);
    for (int i = 0; i < g->numNodes; i++) {
        printf("  [%2d] %-26s (%3d,%3d) ->",
               g->nodes[i].id, g->nodes[i].name, g->nodes[i].x, g->nodes[i].y);
        for (Edge *e = g->adj[i]; e; e = e->next)
            printf(" %d(w=%d%s)", e->destination, e->weight, e->blocked ? ",X" : "");
        printf("\n");
    }
}

const char *nodeType_name(NodeType t) {
    switch (t) {
        case NODE_HOSPITAL: return "HOSPITAL";
        case NODE_STATION:  return "STATION";
        default:            return "JUNCTION";
    }
}
