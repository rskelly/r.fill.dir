#include <stdlib.h>
#include "ds.h"

struct queue* queue_init(void (*deleter)(void*)) {
    struct queue* q = (struct queue*) calloc(1, sizeof(struct queue));
    q->deleter = deleter;
    return q;
}

void queue_free(struct queue* q) {
    queue_clear(q);
    free(q);
}

void* queue_pop(struct queue* q) {
    struct node* n = 0;
    void* v = 0;
    if(q->head) {
        n = q->head;
        if(q->head == q->tail) {
            q->head = q->tail = 0;
        } else {
            q->head = q->head->next;
        }
        --q->size;
    }
    if(n) {
        v = n->value;
        free(n);
    }
    return v;
}

int queue_push(struct queue* q, void* value) {
    struct node* n = (struct node*) calloc(1, sizeof(struct node));
    n->value = value;
    if(!q->tail) {
        q->head = q->tail = n;
    } else {
        q->tail->next = n;
        q->tail = n;
    }
    return ++q->size;
}

int queue_insert(struct queue* q, void* value) {
    struct node* n = (struct node*) calloc(1, sizeof(struct node));
    n->value = value;
    if(!q->head) {
        q->head = q->tail = n;
    } else {
        n->next = q->head;
        q->head = n;
    }
    return ++q->size;
}

void queue_clear(struct queue* q) {
    struct node* n;
    while((n = queue_pop(q))) {
        q->deleter(n->value);
        free(n);
    }
    q->size = 0;
}

int queue_size(struct queue* q) {
    return q->size;
}

int queue_empty(struct queue* q) {
    return q->size == 0;
}

void q_deleter(void* item) {
    free(item);
}