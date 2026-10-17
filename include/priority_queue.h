#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

/*
 * Priority Queue ADT for EMERGENCIES, implemented as a binary max-heap.
 * Priority rule (pq_higherPriority):
 *   1. severity DESC      -> CRITICAL served first
 *   2. tie-break          -> older emergency (smaller createdAt) first
 * push/pop: O(log n), peek: O(1), space: O(n)
 */
typedef struct PQItem {
    int emergencyId;
    int severity;
    long createdAt;
} PQItem;

typedef struct PriorityQueue {
    PQItem *items;
    int size;
    int capacity;
} PriorityQueue;

void pq_init(PriorityQueue *pq);
void pq_free(PriorityQueue *pq);
int  pq_push(PriorityQueue *pq, int emergencyId, int severity, long createdAt);
int  pq_pop(PriorityQueue *pq, PQItem *out);              /* 0 if empty */
int  pq_peek(const PriorityQueue *pq, PQItem *out);
int  pq_remove(PriorityQueue *pq, int emergencyId);       /* cancel/re-prioritize */
int  pq_size(const PriorityQueue *pq);
int  pq_higherPriority(const PQItem *a, const PQItem *b);

#endif
