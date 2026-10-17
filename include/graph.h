#ifndef GRAPH_H
#define GRAPH_H

#include "types.h"

/*
 * City road network as an UNDIRECTED weighted graph.
 * Representation: adjacency list (array of linked lists of Edge).
 * Node ids are 0-based sequential, so id == array index.
 * Weight = current travel time (changes with traffic).
 */
typedef struct Graph {
    Node *nodes;
    Edge **adj;
    int numNodes;
    int capacity;
} Graph;

int   graph_init(Graph *g, int capacity);
void  graph_free(Graph *g);
int   graph_addNode(Graph *g, const char *name, int x, int y, NodeType type);
Node *graph_getNode(const Graph *g, int id);
int   graph_addEdge(Graph *g, int from, int to, int weight);
Edge *graph_findEdge(const Graph *g, int from, int to);
int   graph_setEdgeWeight(Graph *g, int from, int to, int weight);
int   graph_setEdgeBlocked(Graph *g, int from, int to, int blocked);
int   graph_edgeWeight(const Graph *g, int from, int to, int *weight);
void  graph_print(const Graph *g);
const char *nodeType_name(NodeType t);

#endif

