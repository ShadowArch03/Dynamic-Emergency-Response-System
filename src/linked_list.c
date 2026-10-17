#include <stdlib.h>
#include "linked_list.h"

void list_init(List *list) {
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}

int list_push_back(List *list, void *data) {
    ListNode *node = malloc(sizeof(ListNode));
    if (!node) return -1;                 /* allocation failure: reported, not crashed */
    node->data = data;
    node->next = NULL;
    if (list->tail) list->tail->next = node;
    else list->head = node;
    list->tail = node;
    list->count++;
    return 0;
}

void *list_remove(List *list, const void *data) {
    ListNode **indirect = &list->head;
    while (*indirect) {
        if ((*indirect)->data == data) {
            ListNode *victim = *indirect;
            void *payload = victim->data;
            *indirect = victim->next;     /* unlink */
            if (list->tail == victim) {   /* removed the tail: recompute it */
                list->tail = NULL;
                ListNode *cur = list->head;
                while (cur) { list->tail = cur; cur = cur->next; }
            }
            free(victim);
            list->count--;
            return payload;
        }
        indirect = &(*indirect)->next;
    }
    return NULL;                          /* not found */
}

void *list_find(const List *list, ListPredicate pred, void *context) {
    if (!pred) return NULL;               /* hardening: NULL callback guard */
    for (ListNode *n = list->head; n; n = n->next)
        if (pred(n->data, context)) return n->data;
    return NULL;
}

void list_clear(List *list, ListDestructor dtor) {
    ListNode *n = list->head;
    while (n) {
        ListNode *next = n->next;
        if (dtor) dtor(n->data);
        free(n);
        n = next;
    }
    list->head = list->tail = NULL;
    list->count = 0;
}

int list_size(const List *list) {
    return list->count;
}
