#include "csapp.h"
#include "cache.h"

/* functions about cache */
void cache_init(cache_t *cache) {
    Sem_init(&cache->sem_queue, 1, 1);
    cache->head = NULL;
    cache->tail = NULL;
    cache->cache_size = 0;
}

node_t *search_cache(cache_t *cache, char *uri, char *path) {
    node_t *handle = cache->head;

    while(handle) {
        if ((!strcmp(uri, handle->uri)) && (!strcmp(path, handle->path))) {
            requeue(cache, handle);
            return handle;
        }
        handle = handle->next;
    }
    return NULL;
}

/* must enter with lock held */
void enqueue(cache_t *cache, node_t *target) {
    while(cache->cache_size + target->obj_size > MAX_OBJECT_SIZE) {
        /* eviction necessary */
        node_t *eviction_target = dequeue(cache);
        node_del(eviction_target);
    }
    if (!cache->tail) {
        /* empty queue */
        cache->head = target;
        cache->tail = target;
        target->next = NULL;
        target->prev = NULL;
    } else {
        cache->tail->next = target;
        target->prev = cache->tail;
        cache->tail = target;
        cache->cache_size += target->obj_size;
    }
}

/* must enter with lock held */
node_t *dequeue(cache_t *cache) {
    node_t *temp = cache->head;
    cache->head = temp->next;
    cache->cache_size -= temp->obj_size;
    return temp;
}

/* must enter with lock held */
void remove_node(cache_t *cache, node_t *target) {
    node_t *prev = target->prev;
    node_t *next = target->next;
    if (prev) {
        prev->next = next;
    }
    if (next) {
        next->prev = prev;
    }
}

void requeue(cache_t *cache, node_t *target) {
    P(&cache->sem_queue);
    remove_node(cache, target);
    enqueue(cache, target);
    V(&cache->sem_queue);
}

void node_init(cache_t *cache, char *uri, char *path, char *buf, size_t buf_size) {
    if (buf_size > MAX_OBJECT_SIZE) {
        return;
    }

    uri_len = strlen(uri);
    path_len = strlen(path);


    node_t *new_node = Malloc(sizeof(node_t));
    new_node->uri = Malloc(uri_len);
    new_node->path = Malloc(path_len);
    strncpy(new_node->uri, uri, uri_len);
    strncpy(new_node->path, path_len);
    new_node->obj = Malloc(buf_size);
    strncpy(new_node->obj, buf, buf_size);
    new_node->obj_size = buf_size;
    new_node->prev = NULL;
    new_node->next = NULL;

    P(&cache->sem_queue);
    enqueue(cache, new_node);
    V(&cache->sem_queue);
}

void node_del(node_t *target) {
    Free(target->obj);
    Free(target->uri);
    Free(target->path);
    Free(target);
}