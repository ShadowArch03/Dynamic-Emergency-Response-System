#ifndef LINKED_LIST_H
#define LINKED_LIST_H

/*
 * Generic singly linked list.
 * Caller OWNS the data; the list only stores pointers.
 * Used for: road / hospital / ambulance / emergency registries
 * and the report log. (Graph adjacency lists are separate typed
 * Edge lists inside graph.c.)
 */
typedef struct ListNode {
    void *data;
    struct ListNode *next;
} ListNode;

typedef struct List {
    ListNode *head;
    ListNode *tail;          /* O(1) push_back */
    int count;
} List;

typedef void (*ListDestructor)(void *data);
typedef int  (*ListPredicate)(void *data, void *context);

void  list_init(List *list);
int   list_push_back(List *list, void *data);
void *list_remove(List *list, const void *data);   /* unlink; caller frees data */
void *list_find(const List *list, ListPredicate pred, void *context);
void  list_clear(List *list, ListDestructor dtor);
int   list_size(const List *list);

#endif
