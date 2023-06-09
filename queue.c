#include <threads.h>
#include <stdlib.h>
#include "queue.h"

typedef struct list_item{
    void *item;
    struct list_item *next;
}list_item;

typedef struct list_cv{
    cnd_t *cv;
    struct list_cv *next;
    void *to_deq;//the item that the thread would dequeue; 
}list_cv;

mtx_t mtx;
cnd_t waiting_list;
size_t size_counter;
size_t waiting_counter;
size_t visited_counter;
list_item *available_items;//pointer for the struct list_item that holds first item waiting in the queue without any thread assigned to it; 
list_cv *waiting_threads;//pointer for the struct list_cv that holds the cv in which the first thread is waiting for an item that will be assigned to it to dequeue;
mtx_t lock;


void insert_to_available_items(void *item){
    list_item *curr;
    if (available_items == NULL){
        available_items = (list_item*) malloc(sizeof(list_item));
        available_items->item = item;
    }
    else{   
        curr = available_items;
        while (curr->next != NULL){
            curr = curr->next;
        }
        curr->next = malloc(sizeof(list_item));
        (curr->next)->item = item;
    }
}

void insert_to_waiting_threads(cnd_t *cv){
    list_cv *curr;
    if (waiting_threads == NULL){
        waiting_threads = (list_cv*) malloc(sizeof(list_cv));
        waiting_threads->cv = cv;
    }
    else{
        curr = waiting_threads;
        while (curr->next != NULL){
            curr = curr->next;
        }
        curr->next = malloc(sizeof(list_item));
        (curr->next)->cv = cv;
    }
}

void* extract_first_available_item_and_free_node(void){
    list_item *tmp_list_item;
    void* res;
    tmp_list_item = available_items;
    available_items = available_items->next;
    res = tmp_list_item->item;
    free(tmp_list_item);
    return res;
}

void free_available_list(list_item *head){
    list_item *curr;
    curr = head;
    while (curr != NULL){
        free_available_list(curr->next);
        free(curr);
    }
}

void free_waiting_list(list_cv *head){
    list_cv *curr;
    curr = head;
    while (curr != NULL){
        free_waiting_list(curr->next);
        free(curr);
    }
}

void initQueue(void){
    available_items = NULL;
    waiting_threads = NULL;
    mtx_init(&lock, mtx_plain);
    waiting_counter = 0;
    visited_counter = 0;
    size_counter = 0;
}
void destroyQueue(void){
    mtx_lock(&lock);
    free_available_list(available_items);
    free_waiting_list(waiting_threads);
    mtx_unlock(&lock);
    mtx_destroy(&lock);
}

void enqueue(void* item){
    mtx_lock(&lock);
    ++size_counter;
    if (waiting_threads == NULL){
         insert_to_available_items(item);
    }
    else{
        waiting_threads->to_deq = item;
        cnd_signal(waiting_threads->cv);
    }
    mtx_unlock(&lock);
}

void* dequeue(void){
    void *res;
    cnd_t cv;
    list_cv *tmp_list_cv;
    mtx_lock(&lock);
    cnd_init(&cv);
    if (available_items != NULL){
        res = extract_first_available_item_and_free_node();
        size_counter = size_counter-1;
        ++visited_counter;
    }
    else{
        ++waiting_counter;
        insert_to_waiting_threads(&cv);
        cnd_wait(&cv, &lock);
        waiting_counter = waiting_counter-1;
        size_counter = size_counter-1;// it's duplicated action in both two conditions because I wanted to update the counters as close as possible to the removing action
        ++visited_counter;
        tmp_list_cv = waiting_threads;
        waiting_threads = waiting_threads->next;
        res = tmp_list_cv->to_deq;
        free(tmp_list_cv);
        
    }
    mtx_unlock(&lock);
    cnd_destroy(&cv);
    return res;
}

bool tryDequeue(void** ptr){
    void *res;
    mtx_lock(&lock);
    if (available_items == NULL){
        mtx_unlock(&lock);
        return false;
    }
    res = extract_first_available_item_and_free_node();
    size_counter = size_counter -1; 
    ++visited_counter;
    mtx_unlock(&lock);
    *ptr = res;
    return true;
}

size_t size(void){
    return size_counter;
}
size_t waiting(void){
    return waiting_counter;
}
size_t visited(void){
    return visited_counter;
}