#include <stdlib.h>
#include "priority_queue.h"

int pq_higherPriority(const PQItem *a, const PQItem *b) {
    if (a->severity != b->severity) return a->severity > b->severity;
    return a->createdAt < b->createdAt;      /* older first */
}

static void siftUp(PriorityQueue *pq, int i) {
    while (i > 0) {
        int p = (i - 1) / 2;
        if (!pq_higherPriority(&pq->items[i], &pq->items[p])) break;
        PQItem tmp = pq->items[i];
        pq->items[i] = pq->items[p];
        pq->items[p] = tmp;
        i = p;
    }
}

static void siftDown(PriorityQueue *pq, int i) {
    for (;;) {
        int l = 2 * i + 1, r = 2 * i + 2, m = i;
        if (l < pq->size && pq_higherPriority(&pq->items[l], &pq->items[m])) m = l;
        if (r < pq->size && pq_higherPriority(&pq->items[r], &pq->items[m])) m = r;
        if (m == i) break;
        PQItem tmp = pq->items[i];
        pq->items[i] = pq->items[m];
        pq->items[m] = tmp;
        i = m;
    }
}

void pq_init(PriorityQueue *pq) {
    pq->items = NULL;
    pq->size = 0;
    pq->capacity = 0;
}

void pq_free(PriorityQueue *pq) {
    free(pq->items);
    pq_init(pq);
}

int pq_push(PriorityQueue *pq, int emergencyId, int severity, long createdAt) {
    if (pq->size == pq->capacity) {                 /* grow: 16, 32, 64 ... */
        int newCap = pq->capacity ? pq->capacity * 2 : 16;
        PQItem *ni = realloc(pq->items, (size_t)newCap * sizeof(PQItem));
        if (!ni) return -1;                         /* OOM: original intact */
        pq->items = ni;
        pq->capacity = newCap;
    }
    pq->items[pq->size].emergencyId = emergencyId;
    pq->items[pq->size].severity = severity;
    pq->items[pq->size].createdAt = createdAt;
    pq->size++;
    siftUp(pq, pq->size - 1);
    return 0;
}

int pq_pop(PriorityQueue *pq, PQItem *out) {
    if (pq->size == 0) return 0;
    if (out) *out = pq->items[0];
    pq->items[0] = pq->items[--pq->size];
    if (pq->size > 0) siftDown(pq, 0);
    return 1;
}

int pq_peek(const PriorityQueue *pq, PQItem *out) {
    if (pq->size == 0) return 0;
    if (out) *out = pq->items[0];
    return 1;
}

/* Remove by emergencyId from ANYWHERE in the heap (used for cancel /
   severity change later). Self-assign-safe when removing the last slot. */
int pq_remove(PriorityQueue *pq, int emergencyId) {
    for (int i = 0; i < pq->size; i++) {
        if (pq->items[i].emergencyId == emergencyId) {
            pq->items[i] = pq->items[pq->size - 1];   /* replace with last */
            pq->size--;
            if (i < pq->size) {
                siftDown(pq, i);    /* the swapped-in element may need to  */
                siftUp(pq, i);      /* travel in EITHER direction          */
            }
            return 1;
        }
    }
    return 0;
}

int pq_size(const PriorityQueue *pq) {
    return pq->size;
}
