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
        available_items->next = NULL;
    }
    else{   
        curr = available_items;
        while (curr->next != NULL){
            curr = curr->next;
        }
        curr->next = (list_item*) malloc(sizeof(list_item));
        (curr->next)->item = item;
        (curr->next)->next = NULL;
    }
}

list_cv* insert_to_waiting_threads(cnd_t *cv){
    list_cv *curr;
    curr = waiting_threads;
    if (curr == NULL){
        waiting_threads = (list_cv*) malloc(sizeof(list_cv));
        waiting_threads->cv = cv;
        waiting_threads->to_deq = NULL;
        waiting_threads->next = NULL;
        return waiting_threads;
    }
    while (curr->next != NULL){
        curr = curr->next;
    }
    curr->next = (list_cv*) malloc(sizeof(list_cv));
    (curr->next)->cv = cv;
    (curr->next)->next = NULL;
    (curr->next)->to_deq = NULL;
    return curr->next;
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
    if (head != NULL){
        free_available_list(head->next);
        free(head);
    }
}

void free_waiting_list(list_cv *head){
    if (head != NULL){
        free_waiting_list(head->next);
        free(head);
    }
}

void initQueue(void){
    mtx_init(&lock, mtx_plain);
    mtx_lock(&lock);
    waiting_counter = 0;
    visited_counter = 0;
    size_counter = 0;
    available_items = NULL;
    waiting_threads = NULL;
    mtx_unlock(&lock);
}

void destroyQueue(void){
    mtx_lock(&lock);
    free_available_list(available_items);
    free_waiting_list(waiting_threads);
    mtx_destroy(&lock);
}

void enqueue(void* item){
    list_cv *tmp;
    mtx_lock(&lock);
    ++size_counter;
    if (waiting_threads == NULL){
         insert_to_available_items(item);
    }
    else{
        waiting_threads->to_deq = item;
        tmp = waiting_threads;
        waiting_threads = waiting_threads->next;
        cnd_signal(tmp->cv);
    }
    mtx_unlock(&lock);
}

void* dequeue(void){
    void *res;
    cnd_t cv;
    list_cv *active_node;
    mtx_lock(&lock);
    cnd_init(&cv);
    if (available_items != NULL){
        res = extract_first_available_item_and_free_node();
        size_counter = size_counter-1;
        ++visited_counter;
    }
    else{
        ++waiting_counter;
        active_node = insert_to_waiting_threads(&cv);
        cnd_wait(&cv, &lock);
        waiting_counter = waiting_counter-1;
        res = active_node->to_deq;
        if (res != NULL){
            size_counter = size_counter-1;
            ++visited_counter;
        }
        free(active_node);
    }
    cnd_destroy(&cv);
    mtx_unlock(&lock);
    
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
