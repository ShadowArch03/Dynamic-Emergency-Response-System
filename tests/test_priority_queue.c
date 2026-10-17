#include <assert.h>
#include <stdio.h>
#include "priority_queue.h"
#include "types.h"

int main(void) {
    PriorityQueue pq;
    pq_init(&pq);
    PQItem it;

    /* empty behaviours */
    assert(pq_size(&pq) == 0);
    assert(!pq_pop(&pq, &it));
    assert(!pq_peek(&pq, &it));

    /* severity first; equal severity -> older (smaller createdAt) first */
    pq_push(&pq, 101, SEV_LOW, 10);
    pq_push(&pq, 102, SEV_CRITICAL, 12);
    pq_push(&pq, 103, SEV_HIGH, 11);
    pq_push(&pq, 104, SEV_CRITICAL, 15);
    assert(pq_size(&pq) == 4);

    assert(pq_peek(&pq, &it) && it.emergencyId == 102);
    assert(pq_pop(&pq, &it) && it.emergencyId == 102);   /* critical, older */
    assert(pq_pop(&pq, &it) && it.emergencyId == 104);   /* critical, newer */
    assert(pq_pop(&pq, &it) && it.emergencyId == 103);
    assert(pq_pop(&pq, &it) && it.emergencyId == 101);
    assert(pq_size(&pq) == 0);

    /* equal severity: strict FIFO by creation time */
    for (int id = 1; id <= 5; id++) pq_push(&pq, id, SEV_MEDIUM, id * 10);
    for (int id = 1; id <= 5; id++)
        assert(pq_pop(&pq, &it) && it.emergencyId == id);

    /* remove from the middle keeps order */
    for (int id = 1; id <= 5; id++) pq_push(&pq, id, SEV_MEDIUM, id);
    assert(pq_remove(&pq, 3) == 1);
    assert(pq_remove(&pq, 99) == 0);                     /* not present */
    assert(pq_size(&pq) == 4);
    assert(pq_pop(&pq, &it) && it.emergencyId == 1);
    assert(pq_pop(&pq, &it) && it.emergencyId == 2);
    assert(pq_pop(&pq, &it) && it.emergencyId == 4);
    assert(pq_pop(&pq, &it) && it.emergencyId == 5);
    assert(pq_size(&pq) == 0);

    /* remove the ROOT */
    pq_push(&pq, 10, SEV_LOW, 1);
    pq_push(&pq, 11, SEV_CRITICAL, 2);
    pq_push(&pq, 12, SEV_HIGH, 3);
    assert(pq_remove(&pq, 11) == 1);
    assert(pq_peek(&pq, &it) && it.emergencyId == 12);   /* high now on top */

    /* remove the LAST remaining element */
    pq_remove(&pq, 10);
    assert(pq_remove(&pq, 12) == 1);
    assert(pq_size(&pq) == 0);
    assert(!pq_pop(&pq, &it));

    /* capacity growth + global order check (100 items) */
    for (int id = 1; id <= 100; id++) pq_push(&pq, id, id % 4, id);
    int prevSev = 3;
    long prevTime = -1;
    int count = 0;
    while (pq_pop(&pq, &it)) {
        assert(it.severity <= prevSev);                  /* non-increasing sev   */
        if (it.severity == prevSev)
            assert(it.createdAt > prevTime);             /* FIFO within severity */
        prevSev = it.severity;
        prevTime = it.createdAt;
        count++;
    }
    assert(count == 100);

    pq_free(&pq);
    printf("test_priority_queue: ALL PASSED\n");
    return 0;
}
