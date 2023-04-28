#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

// ***********************************************************
// helper function prototypes
// ***********************************************************
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *allocate(void *bp, size_t blk_size);
static void insert_node(void *bp, size_t blk_size);
static void *find_fitting_blk(size_t blk_size);
static void remove_node(void *bp);


// ***********************************************************
// MACROs
// ***********************************************************

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

/* minimum block size : space for header, footer, prev ptr, next ptr (for freed block) and for alignment*/
#define MINIMUM_BLK_SIZE 16

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/* read WSIZE bytes from pointer p */
#define GET_W(p) (*((unsigned int *) (p)))

/* write WSIZE bytes of value 'val' to pointer p */
#define PUT_W(p, val) (*((unsigned int *) (p)) = (val)) 

/* getting size of the block from header or footer at address p */
#define GET_SIZE(p) (GET_W(p) & ~0x7)

/* getting allocated bit from header or footer at address p */
#define GET_ISALLOCATED(p) (GET_W(p) & 0x1)

/* given block pointer bp, compute address of its header and footer */
#define HDRP(bp) (((char *) (bp)) - WSIZE)
#define FTRP(bp) (((char *) (bp)) + GET_SIZE(HDRP(bp)) - DSIZE)

/* given block pointer bp, return next / prev block pointer */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define MAX(x, y) (x) > (y) ? x : y
#define MIN(x, y) (x) < (y) ? x : y

/* number of segregated list */
#define NUM_SEG_LIST 10

/* get prev, next for free block given bp */
#define GET_PREV_PTR(bp) (*((unsigned int *) bp))
#define GET_NEXT_PTR(bp) (*(((unsigned int *) bp) + 1))

/* set prev, next for free block given bp */
#define SET_PREV_PTR(bp, addr) (GET_PREV_PTR(bp) = (unsigned int) (addr))
#define SET_NEXT_PTR(bp, addr) (GET_NEXT_PTR(bp) = (unsigned int) (addr))

/* standard of blk size. when to start from head, when to start from tail? */
#define STANDARD 2048

// #define DEBUG 1

#ifdef DEBUG
static int mm_check();
/* heap consistency checker */
#define MM_CHECK mm_check()
#endif

// ***********************************************************
// global static variables
// ***********************************************************

/* pointer which points to middle of the prologue block */
static char *heap_listp;
/* segregated list to manage free blocks */
static void *free_list_head;
static void *free_list_tail;


// ***********************************************************
// helper functions start
// ***********************************************************

/*
 * extend_heap - Extend heap by 'words'. Coalesce if previous block was free.
 * return bp of the extended heap.
 */ 
static void* extend_heap(size_t words) 
{
    char* bp;

    int aligned_words = words % 2 == 0 ? words : words + 1;

    size_t extend_size = aligned_words * WSIZE;

    if ((bp = (char *) mem_sbrk(extend_size)) == (char *) -1) {
        return NULL;
    }

    PUT_W(HDRP(bp), PACK(extend_size, 0));
    PUT_W(FTRP(bp), PACK(extend_size, 0));
    PUT_W(FTRP(bp) + WSIZE, PACK(0, 1)); /* set the epilogue block */

    return coalesce(bp);
}

/* coalesce with next or prev block. Remove coalesced block from the freelist. Return bp after coalesce */
static void* coalesce(void *bp) 
{
    char* next_blkp = NEXT_BLKP(bp);
    char* prev_blkp = PREV_BLKP(bp);
    size_t size = GET_SIZE(HDRP(bp));
    size_t size_after_coalesce = size;

    int is_next_allocated = GET_ISALLOCATED(HDRP(next_blkp));
    int is_prev_allocated = GET_ISALLOCATED(HDRP(prev_blkp));

    if (!is_next_allocated) { /* if continuihng block is free => coalesce */
        size_after_coalesce += GET_SIZE(HDRP(next_blkp));
        
        remove_node(next_blkp);
        PUT_W(HDRP(bp), PACK(size_after_coalesce, 0));
        PUT_W(FTRP(bp), PACK(size_after_coalesce, 0));
    }

    if (!is_prev_allocated) { /* if preceding block is free => coalesce*/
        size_after_coalesce += GET_SIZE(HDRP(prev_blkp));

        remove_node(prev_blkp);
        PUT_W(HDRP(prev_blkp), PACK(size_after_coalesce, 0));
        PUT_W(FTRP(bp), PACK(size_after_coalesce, 0));

        bp = (void *) prev_blkp;
    }

    insert_node(bp, GET_SIZE(HDRP(bp))); /* insert the coalesced block */

    return (void *) bp;
}

/* mark given block (bp) as allocated. split if possible. 
 * add the splitted blk to the freelist, remove allocated blk from the freelist.
 */
static void *allocate(void *bp, size_t blk_size)
{
    remove_node(bp);

    size_t curr_blk_size = GET_SIZE(HDRP(bp));

    if (curr_blk_size - blk_size > MINIMUM_BLK_SIZE) {  /* split possible */
        if (blk_size >= 100) { /* if the block is big enough: put it at the back of the block */
            PUT_W(HDRP(bp), PACK(curr_blk_size - blk_size, 0));
            PUT_W(FTRP(bp), PACK(curr_blk_size - blk_size, 0));
            char *next_blkp = NEXT_BLKP(bp);
            PUT_W(HDRP(next_blkp), PACK(blk_size, 1));
            PUT_W(FTRP(next_blkp), PACK(blk_size, 1));
            insert_node(bp, curr_blk_size - blk_size);
            return next_blkp;
        } else {
            PUT_W(HDRP(bp), PACK(blk_size, 1));
            PUT_W(FTRP(bp), PACK(blk_size, 1));
            char *next_blkp = NEXT_BLKP(bp);
            PUT_W(HDRP(next_blkp), PACK(curr_blk_size - blk_size, 0));
            PUT_W(FTRP(next_blkp), PACK(curr_blk_size - blk_size, 0));
            insert_node(next_blkp, curr_blk_size - blk_size);
        }
    } else { /* too small to split*/
        PUT_W(HDRP(bp), PACK(curr_blk_size, 1));
        PUT_W(FTRP(bp), PACK(curr_blk_size, 1));
    }

    return bp;
}

/* add freed block at bp with size blk_size to appropriate position of the free list */
static void insert_node(void *bp, size_t blk_size) 
{
    void *prev = NULL;
    void *next = NULL;

    if (blk_size >= STANDARD) { /* big block: search from the tail of the list */
        prev = free_list_tail;

        while (prev && (blk_size < GET_SIZE(HDRP(prev)))) {
            next = prev;
            prev = (void *)GET_PREV_PTR(prev);
        }
    } else { /* small block: search from the head of the list */
        next = free_list_head;

        while (next && (blk_size > GET_SIZE(HDRP(next)))) {
            prev = next;
            next = (void *)GET_NEXT_PTR(next);
        }
    }
        
    if (prev) {
        if (next) {
            SET_NEXT_PTR(bp, next);
            SET_PREV_PTR(bp, prev);
            SET_NEXT_PTR(prev, bp);
            SET_PREV_PTR(next, bp);
        } else {
            /* bp is biggest. add to the tail of the free list */
            SET_NEXT_PTR(bp, NULL);
            SET_PREV_PTR(bp, prev);
            SET_NEXT_PTR(prev, bp);
            free_list_tail = bp;
        }
    } else {
        if (next) {
            /* bp is smallest. add at the head of the free list */
            SET_PREV_PTR(bp, NULL);
            SET_NEXT_PTR(bp, next);
            SET_PREV_PTR(next, bp);
            free_list_head = bp;
        } else {
            /* when the free list was empty */
            free_list_tail = bp;
            free_list_head = bp;
            SET_NEXT_PTR(bp, NULL);
            SET_PREV_PTR(bp, NULL);
        }
    }
}

/* select free block of size blk_size to allocate */
static void *find_fitting_blk(size_t blk_size) 
{
    char* bp;

    if (blk_size >= STANDARD) {
        /* search from back of the free list */
        bp = free_list_tail;
        while(bp && (blk_size > GET_SIZE(HDRP(bp)))) {
            bp = (char *)GET_PREV_PTR(bp);
        }   
    } else {
        /* search from head of the free list */
        bp = free_list_head;
        while (bp && (blk_size > GET_SIZE(HDRP(bp)))) {
            bp = (char *)GET_NEXT_PTR(bp);
        }
    }

    
    if (!bp) { //no matching block found 
        int extend_size = MAX(blk_size, CHUNKSIZE);
        return extend_heap(extend_size/WSIZE);
    } else { //found
        return bp;
    }
}

/*
removing node from linked list (free list)
*/
static void remove_node(void *bp) 
{
    if (GET_PREV_PTR(bp)) {
		if (GET_NEXT_PTR(bp)) { /* there exist prev and next */
            SET_NEXT_PTR(GET_PREV_PTR(bp), GET_NEXT_PTR(bp));
            SET_PREV_PTR(GET_NEXT_PTR(bp), GET_PREV_PTR(bp));
		} else { /* there exist prev, no next => bp is the last block of the free list */
            SET_NEXT_PTR(GET_PREV_PTR(bp), NULL);
			free_list_tail = (void *) GET_PREV_PTR(bp);
		}
	} else { 
		if (GET_NEXT_PTR(bp)) { /* no prev, exist next => bp is the first block of the free list */
            SET_PREV_PTR(GET_NEXT_PTR(bp), NULL);
            free_list_head = (void *) GET_NEXT_PTR(bp);
        } else { /* the only block in the free list removed */
			free_list_tail = NULL;
            free_list_head = NULL;
        }
	}
}

// ***********************************************************
// helper functions end
// ***********************************************************

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = (char *) mem_sbrk(4 * WSIZE)) == (char *) (-1)) {
        return -1;
    }

    PUT_W(heap_listp, 0);
    PUT_W(heap_listp + WSIZE, PACK(DSIZE, 1));
    PUT_W(heap_listp + 2 * WSIZE, PACK(DSIZE, 1));
    PUT_W(heap_listp + 3 * WSIZE, PACK(0, 1));
    heap_listp += DSIZE;

    /* initialize free_list */
    free_list_head = NULL;
    free_list_tail = NULL;

    void *bp; 

    if ((bp = extend_heap(CHUNKSIZE/WSIZE)) == NULL) {
        return -1;
    }

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    int adjusted_size = ALIGN(size + DSIZE); /* necessary space considering alignment, header and footer */

    void *bp;

    if ((bp = find_fitting_blk(adjusted_size)) == NULL) {
        return NULL;
    }

    
    bp = allocate(bp, adjusted_size);

#ifdef DEBUG
    if (!MM_CHECK) {
        printf("mm_check failed\n");
    }
#endif

    return bp;
}

/*
 * mm_free - Freeing a block. 
 * Gets bp as an input and frees the block 
 */
void mm_free(void *ptr)
{
    char* hdrp = HDRP(ptr);
    char* ftrp = FTRP(ptr);
    size_t curr_block_size = GET_SIZE(hdrp);

    PUT_W(hdrp, PACK(curr_block_size, 0));
    PUT_W(ftrp, PACK(curr_block_size, 0));
    coalesce(ptr);

#ifdef DEBUG
    if (!MM_CHECK) {
        printf("mm_check failed\n");
    }
#endif
}

/*
 * mm_realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *newptr = ptr;
	size_t new_blk_size;
	size_t extend_size;
	int remaining_size;

	if (size == 0) {
		return NULL;
    }

    /* for alignment conditions */
	if (size <= DSIZE) {
        new_blk_size = 2*DSIZE;
    } else {
		new_blk_size = ALIGN(size + DSIZE);
    }

	if (GET_SIZE(HDRP(ptr)) < new_blk_size) {
        remaining_size = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - new_blk_size;

		if (!GET_ISALLOCATED(HDRP(NEXT_BLKP(ptr)))) { /* if next block free */
			if (remaining_size >= 0) {
                remove_node(NEXT_BLKP(ptr));
                PUT_W(HDRP(ptr), PACK(new_blk_size + remaining_size, 1));
                PUT_W(FTRP(ptr), PACK(new_blk_size + remaining_size, 1));
            }
		} else if (GET_SIZE(HDRP(ptr)) == 0 && (remaining_size < 0)) {
            /* extend heap */
            extend_size = MAX(-remaining_size, CHUNKSIZE);
            if (extend_heap(extend_size/WSIZE) == NULL) {
                return NULL;
            }

            remaining_size += extend_size; /* now, new_blk_size + remaining_size == (initial size of the block) + (extended size) */

            PUT_W(HDRP(ptr), PACK(new_blk_size + remaining_size, 1)); 
            PUT_W(FTRP(ptr), PACK(new_blk_size + remaining_size, 1)); 
        } else {
			newptr = mm_malloc(new_blk_size - DSIZE); /* -DSIZE for header and footer */
			memcpy(newptr, ptr, MIN(size, new_blk_size));
			mm_free(ptr);
		}
	}

    return newptr;
}

#ifdef DEBUG
static int mm_check()
{
	void *bp;

	for (bp = free_list_head; bp; bp = (void *) GET_NEXT_PTR(bp)) {
        if (GET_ISALLOCATED(HDRP(bp))) { /* check if every block in the free list is free */
            printf("free block allocated at %u\n", (unsigned int) bp);
            return 0;
        }

        /* visualize blocks in the free list */
        printf("[ curr: %u, size: %d, prev: %u, next: %u ] ", (unsigned int) bp, GET_SIZE(HDRP(bp)), GET_PREV_PTR(bp), GET_NEXT_PTR(bp));
    }
    printf("\n");

	/* Check if every free blocks are coalesced properly */
	for (bp = free_list_head; bp; bp = (void *) GET_NEXT_PTR(bp)) {
		if ((void *)GET_PREV_PTR(bp) == PREV_BLKP(bp)) {
            printf("coalesce failed\n");
            return 0;
        }
    }

	return 1;
}
#endif