#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dijkstra.h"

/*
 * Dijkstra's shortest path using a binary min-heap keyed by distance.
 * Blocked edges are skipped entirely (a blocked road "does not exist"
 * for routing purposes).
 * Complexity: O((V + E) log V).   Space: O(V + E).
 */

typedef struct { int node; int dist; } HeapItem;
typedef struct { HeapItem *a; int size; int cap; } MinHeap;

static int heap_init(MinHeap *h, int cap) {
    if (cap <= 0) cap = 1;
    h->a = malloc((size_t)cap * sizeof(HeapItem));
    h->size = 0;
    h->cap = cap;
    return h->a ? 0 : -1;
}

static void heap_free(MinHeap *h) {
    free(h->a);
    h->a = NULL; h->size = 0; h->cap = 0;
}

static void heap_swap(HeapItem *x, HeapItem *y) { HeapItem t = *x; *x = *y; *y = t; }

static int heap_push(MinHeap *h, int node, int dist) {
    if (h->size == h->cap) {              /* grows for stale duplicate entries */
        int nc = h->cap ? h->cap * 2 : 16;
        HeapItem *na = realloc(h->a, (size_t)nc * sizeof(HeapItem));
        if (!na) return -1;
        h->a = na; h->cap = nc;
    }
    int i = h->size++;
    h->a[i].node = node; h->a[i].dist = dist;
    while (i > 0) {                       /* sift up */
        int p = (i - 1) / 2;
        if (h->a[p].dist <= h->a[i].dist) break;
        heap_swap(&h->a[p], &h->a[i]);
        i = p;
    }
    return 0;
}

static int heap_pop(MinHeap *h, int *node, int *dist) {
    if (h->size == 0) return 0;
    *node = h->a[0].node; *dist = h->a[0].dist;   /* save before overwrite */
    h->a[0] = h->a[--h->size];
    int i = 0;
    for (;;) {                            /* sift down */
        int l = 2 * i + 1, r = 2 * i + 2, m = i;
        if (l < h->size && h->a[l].dist < h->a[m].dist) m = l;
        if (r < h->size && h->a[r].dist < h->a[m].dist) m = r;
        if (m == i) break;
        heap_swap(&h->a[i], &h->a[m]);
        i = m;
    }
    return 1;
}

Route dijkstra_shortestPath(const Graph *g, int source, int destination) {
    Route result;
    result.success = 0; result.totalCost = -1;
    result.path = NULL; result.pathLength = 0;

    int n = g->numNodes;
    if (n == 0 || source < 0 || source >= n || destination < 0 || destination >= n)
        return result;                    /* invalid input -> safe empty route */

    int *dist = malloc((size_t)n * sizeof(int));
    int *prev = malloc((size_t)n * sizeof(int));
    unsigned char *done = calloc((size_t)n, 1);
    MinHeap heap; heap.a = NULL; heap.size = 0; heap.cap = 0;

    if (!dist || !prev || !done || heap_init(&heap, n) != 0) goto fail;

    for (int i = 0; i < n; i++) { dist[i] = INF_COST; prev[i] = -1; }
    dist[source] = 0;
    heap_push(&heap, source, 0);

    int node, d;
    while (heap_pop(&heap, &node, &d)) {
        if (done[node]) continue;         /* stale heap entry: skip */
        done[node] = 1;
        if (node == destination) break;   /* destination finalized: stop early */
        for (Edge *e = g->adj[node]; e; e = e->next) {
            if (e->blocked) continue;     /* blocked road does not exist for us */
            int nd = d + e->weight;       /* RELAXATION */
            if (nd < dist[e->destination]) {
                dist[e->destination] = nd;
                prev[e->destination] = node;
                if (heap_push(&heap, e->destination, nd) != 0) goto fail;
            }
        }
    }

    if (dist[destination] == INF_COST) goto fail;   /* UNREACHABLE */

    /* reconstruct path backwards via prev[], then it is already in order */
    int len = 0;
    for (int v = destination; v != -1; v = prev[v]) len++;
    result.path = malloc((size_t)len * sizeof(int));
    if (!result.path) goto fail;
    int idx = len - 1;
    for (int v = destination; v != -1; v = prev[v]) result.path[idx--] = v;

    result.pathLength = len;
    result.totalCost = dist[destination];
    result.success = 1;

    free(dist); free(prev); free(done); heap_free(&heap);
    return result;

fail:
    free(dist); free(prev); free(done); heap_free(&heap);
    result.success = 0; result.path = NULL; result.pathLength = 0; result.totalCost = -1;
    return result;
}

void route_free(Route *r) {
    if (!r) return;
    free(r->path);
    r->path = NULL;
    r->pathLength = 0;
    r->success = 0;
    r->totalCost = -1;
}

/* Sum of edge weights from path[startEdge] onward. -1 if any edge
   missing or blocked (i.e. the remaining route is no longer valid). */
int route_costFrom(const Graph *g, const int *path, int len, int startEdge) {
    if (!path || len < 2) return 0;
    if (startEdge < 0) startEdge = 0;
    int total = 0;
    for (int k = startEdge; k + 1 < len; k++) {
        int w;
        if (graph_edgeWeight(g, path[k], path[k + 1], &w) != 0) return -1;
        total += w;
    }
    return total;
}

/* 1 if any edge from startEdge onward is missing or blocked. */
int route_hasBlockedEdge(const Graph *g, const int *path, int len, int startEdge) {
    if (!path || len < 2) return 0;
    if (startEdge < 0) startEdge = 0;
    for (int k = startEdge; k + 1 < len; k++) {
        Edge *e = graph_findEdge(g, path[k], path[k + 1]);
        if (!e || e->blocked) return 1;
    }
    return 0;
}

/* Safe "1->4->0" style rendering into caller buffer. */
void route_toString(const int *path, int len, char *buf, int size) {
    if (!buf || size <= 0) return;
    buf[0] = '\0';
    if (!path) return;
    for (int i = 0; i < len && size > 1; i++) {
        int written = snprintf(buf, (size_t)size, "%s%d", i ? "->" : "", path[i]);
        if (written < 0 || written >= size) break;
        buf += written;
        size -= written;
    }
}
