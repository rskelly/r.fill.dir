#ifndef __DS_H__
#define __DS_H__

struct node {
    struct node* next;
    void* value;
};

struct queue {
    struct node* head;
    struct node* tail;
    void (*deleter)(void*);
    int size;
};

struct queue* queue_init(void (*deleter)(void*));

void queue_free(struct queue* q);

void* queue_pop(struct queue* q);

int queue_push(struct queue* q, void* value);

int queue_insert(struct queue* q, void* value);

void queue_clear(struct queue* q);

int queue_size(struct queue* q);

int queue_empty(struct queue* q);

void q_deleter(void* item);

#endif
