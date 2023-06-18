/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAXURI 1024

typedef struct node {
    char uri[MAXURI];
    char *obj;
    size_t obj_size;
    struct node *next;
    struct node *prev;
} node_t;

typedef struct cache {
    size_t cache_size;
    node_t *head;
    node_t *tail;
    sem_t sem_queue;
} cache_t;

void cache_init(cache_t *cache);
node_t *search_cache(cache_t *cache, char *uri);
void enqueue(cache_t *cache, node_t *target);
node_t *dequeue(cache_t *cache);
void remove_node(cache_t *cache, node_t *target);
void requeue(cache_t *cache, node_t *target);
void node_init(cache_t *cache, char* uri, char *buf, size_t buf_size);
void node_del(node_t *target);