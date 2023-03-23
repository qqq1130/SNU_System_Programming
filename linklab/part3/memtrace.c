//------------------------------------------------------------------------------
//
// memtrace
//
// trace calls to the dynamic memory manager
//
#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memlog.h>
#include <memlist.h>

/******************************************************
 * 또한 할당 되지 않거나 이미 할당 해제된 메모리에 대한 realloc 함수의 호출은 들어오지 않는다고 가정하시고 문제를 푸셔도 됩니다.
*******************************************************/

/**********************
 helper functions
 **********************/
static void print_non_dealloc_blocks(item* list) {
  item* curr = list->next;

  while (curr != NULL) {
    if (curr->cnt > 0) {
      break;
    }

    curr = curr->next;
  }

  if (curr == NULL) {
    return;
  }

  LOG_NONFREED_START();

  while (curr != NULL) {
    if (curr->cnt > 0) {
      LOG_BLOCK(curr->ptr, curr->size, curr->cnt);
    }

    curr = curr->next;
  }
}

//
// function pointers to stdlib's memory management functions
//
static void *(*mallocp)(size_t size) = NULL;
static void (*freep)(void *ptr) = NULL;
static void *(*callocp)(size_t nmemb, size_t size);
static void *(*reallocp)(void *ptr, size_t size);

//
// statistics & other global variables
//
static unsigned long n_malloc  = 0;
static unsigned long n_calloc  = 0;
static unsigned long n_realloc = 0;
static unsigned long n_allocb  = 0;
static unsigned long n_freeb   = 0;
static item *list = NULL;

//
// init - this function is called once when the shared library is loaded
//
__attribute__((constructor))
void init(void)
{
  char *error;

  LOG_START();

  // initialize a new list to keep track of all memory (de-)allocations
  // (not needed for part 1)
  list = new_list();

  // ...
}

//
// fini - this function is called once when the shared library is unloaded
//
__attribute__((destructor))
void fini(void)
{
  // ...

  LOG_STATISTICS(n_allocb, n_allocb/(n_malloc + n_calloc + n_realloc), n_freeb);

  print_non_dealloc_blocks(list);

  LOG_STOP();

  // free list (not needed for part 1)
  free_list(list);
}

// ...

void *malloc(size_t size) {
  char *error;
  void *ptr;

  if (!mallocp) {
    mallocp = dlsym(RTLD_NEXT, "malloc");

    if ((error = dlerror()) != NULL) {
      fputs(error, stderr);
      exit(1);
    }
  }

  ptr = mallocp(size);

  ++n_malloc;
  n_allocb += size;

  LOG_MALLOC(size, ptr);

  alloc(list, ptr, size);

  return ptr;
}

void free(void *ptr) {
  char *error;
  item *target_block;

  if (!freep) {
    freep = dlsym(RTLD_NEXT, "free");
    
    if ((error = dlerror()) != NULL) {
      fputs(error, stderr);
      exit(1);
    }
  }

  LOG_FREE(ptr);

  target_block = find(list, ptr);

  if (target_block != NULL) {
    if (target_block->cnt < 1) {
      //double free
      LOG_DOUBLE_FREE();
    } else {
      //normal free
      --(target_block->cnt);
      n_freeb += target_block->size;
      freep(ptr);
    }
  } else {
    //illegal free
    LOG_ILL_FREE();
  }

  return;
}

void *calloc(size_t nmemb, size_t size) {
  char *error;
  void *ptr;

  if (!callocp) {
    callocp = dlsym(RTLD_NEXT, "calloc");
    if ((error = dlerror()) != NULL) {
      fputs(error, stderr);
      exit(1);
    }
  }

  ptr = callocp(nmemb, size);

  ++n_calloc;
  n_allocb += nmemb * size;

  LOG_CALLOC(nmemb, size, ptr);

  alloc(list, ptr, nmemb * size);

  return ptr;
}

void *realloc(void *ptr, size_t size) {
  char *error;
  void *reallocated_ptr;

  if (!reallocp) {
    reallocp = dlsym(RTLD_NEXT, "realloc");
    if ((error = dlerror()) != NULL) {
      fputs(error, stderr);
      exit(1);
    }
  }

  //deallocate previous block
  dealloc(list, ptr);
  n_freeb += size;

  //allocate new block
  reallocated_ptr = reallocp(ptr, size);
  alloc(list, reallocated_ptr, size);

  ++n_realloc;
  n_allocb += size;

  LOG_REALLOC(ptr, size, reallocated_ptr);

  return reallocated_ptr;
}
