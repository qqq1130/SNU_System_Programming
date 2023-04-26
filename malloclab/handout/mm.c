/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Self-Studying",
    /* First member's full name */
    "CSH",
    /* First member's email address */
    "shyeokchoi@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

// ***********************************************************
// helper function prototypes
// ***********************************************************
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *allocate(void *bp, size_t blk_size);
static void insert_node(void *bp, size_t blk_size);
static void *find_fitting_blk(size_t blk_size);
static void remove_node(void *bp);
static void mm_check();


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

/* set prev, next for free block given bp */

#define SET_NEXT_PTR(bp, addr) (*((unsigned int *) bp) = (unsigned int) (addr))
#define SET_PREV_PTR(bp, addr) (*(((unsigned int *) bp) + 1) = (unsigned int) (addr))

/* get prev, next for free block given bp */
#define GET_NEXT_PTR(bp) ((void *)(*((unsigned int *) bp)))
#define GET_PREV_PTR(bp) ((void *) (*(((unsigned int *) bp) + 1)))

/* heap consistency checker */
#define MM_CHECK mm_check()

// ***********************************************************
// global static variables
// ***********************************************************

/* pointer which points to middle of the prologue block */
static char *heap_listp;
/* segregated list to manage free blocks */
static void *seg_list[NUM_SEG_LIST];

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

/* coalesce with next or prev block. Remove coalesced block from the seglist. Return bp after coalesce */
static void* coalesce(void *bp) 
{
    char* next_blkp = NEXT_BLKP(bp);
    char* prev_blkp = PREV_BLKP(bp);
    size_t size = GET_SIZE(HDRP(bp));
    size_t size_after_coalesce = size;

    int is_next_allocated = GET_ISALLOCATED(HDRP(next_blkp));
    int is_prev_allocated = GET_ISALLOCATED(HDRP(prev_blkp));

    if (!is_next_allocated) {
        size_after_coalesce += GET_SIZE(HDRP(next_blkp));
        
        remove_node(next_blkp);
        PUT_W(HDRP(bp), PACK(size_after_coalesce, 0));
        PUT_W(FTRP(bp), PACK(size_after_coalesce, 0));
    }

    if (!is_prev_allocated) {
        size_after_coalesce += GET_SIZE(HDRP(prev_blkp));

        remove_node(prev_blkp);
        PUT_W(HDRP(prev_blkp), PACK(size_after_coalesce, 0));
        PUT_W(FTRP(bp), PACK(size_after_coalesce, 0));

        bp = (void *) prev_blkp;
    }

    insert_node(bp, GET_SIZE(HDRP(bp)));

    return (void *) bp;
}

/* mark given block (bp) as allocated. split if possible. 
 * add the splitted blk to the seglist, remove allocated blk from the seglist.
 */
static void *allocate(void *bp, size_t blk_size)
{
    size_t curr_blk_size = GET_SIZE(HDRP(bp));

    if (curr_blk_size - blk_size >= MINIMUM_BLK_SIZE) {
        PUT_W(HDRP(bp), PACK(blk_size, 1));
        PUT_W(FTRP(bp), PACK(blk_size, 1));
        char *next_blkp = NEXT_BLKP(bp);
        PUT_W(HDRP(next_blkp), PACK(curr_blk_size - blk_size, 0));
        PUT_W(FTRP(next_blkp), PACK(curr_blk_size - blk_size, 0));
        insert_node(next_blkp, curr_blk_size - blk_size);
    } else {
        PUT_W(HDRP(bp), PACK(curr_blk_size, 1));
        PUT_W(FTRP(bp), PACK(curr_blk_size, 1));
    }

    return bp;
}

/* add freed block at bp with size blk_size to appropriate seglist */
static void insert_node(void *bp, size_t blk_size) 
{
    int list = 0;
    size_t size = blk_size;

    size >>= 4; // minimum block size is 16

    for (; list < NUM_SEG_LIST - 1; ++list) {
        if (size <= 1) {
            break;
        }

        size >>= 1;
    }

    void **seglist_start = &seg_list[list];
    void *new_node = bp;
    void *next_node = seg_list[list];

    SET_PREV_PTR(new_node, seglist_start);
    SET_NEXT_PTR(new_node, next_node);
    SET_NEXT_PTR(seglist_start, new_node); 

    if (next_node) {
        SET_PREV_PTR(next_node, new_node);
    }
    return;

}

/* select free block of size blk_size to allocate */
static void *find_fitting_blk(size_t blk_size) 
{
    int list = 0;
    size_t size = blk_size;
    
    size >>= 4; /* minimum size of a free block is 16. */

    while ((list < NUM_SEG_LIST - 1) && (size > 1)) {
        size >>= 1;
        ++list;
    }

    while (list < NUM_SEG_LIST) {
        void *curr = seg_list[list];

        while (curr) {
            if (GET_SIZE(HDRP(curr)) > blk_size) {
                return curr;
            }

            curr = GET_NEXT_PTR(curr);
        }

        ++list;
    }

    /* reaching here == block not found */
    int extend_size = MAX(blk_size, CHUNKSIZE);
    return extend_heap(extend_size/WSIZE);
}

/*
removing node from linked list (segregated list)
*/
static void remove_node(void *bp) 
{
    void *prev_node = GET_PREV_PTR(bp);
    void *next_node = GET_NEXT_PTR(bp);

    SET_NEXT_PTR(prev_node, next_node);

    if (next_node) {
        SET_PREV_PTR(next_node, prev_node);
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

    for (int i = 0; i < NUM_SEG_LIST; ++i) {
        seg_list[i] = NULL;
    }

    PUT_W(heap_listp, 0);
    PUT_W(heap_listp + WSIZE, PACK(DSIZE, 1));
    PUT_W(heap_listp + 2 * WSIZE, PACK(DSIZE, 1));
    PUT_W(heap_listp + 3 * WSIZE, PACK(0, 1));
    heap_listp += DSIZE;

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

    remove_node(bp);
    return allocate(bp, adjusted_size);
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

	/* get new block size to match alignment conditions */
	if (size <= DSIZE) {
        new_blk_size = 2*DSIZE;
    } else {
		new_blk_size = ALIGN(size + DSIZE);
    }

	if (GET_SIZE(HDRP(ptr)) < new_blk_size) {
		/* if the next block is free or epilogue block */
		if (!GET_ISALLOCATED(HDRP(NEXT_BLKP(ptr))) || GET_SIZE(HDRP(NEXT_BLKP(ptr))) == 0) {
			remaining_size = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - new_blk_size;

			if (remaining_size < 0) { /* extend heap */
				extend_size = MAX(-remaining_size, CHUNKSIZE);
				if (extend_heap(extend_size/WSIZE) == NULL) {
                    return NULL;
                }
				remaining_size += extend_size;
			}

			remove_node(NEXT_BLKP(ptr));
			PUT_W(HDRP(ptr), PACK(new_blk_size + remaining_size, 1)); 
			PUT_W(FTRP(ptr), PACK(new_blk_size + remaining_size, 1)); 
		} else {
			newptr = mm_malloc(new_blk_size - DSIZE);
			memcpy(newptr, ptr, MIN(size, new_blk_size));
			mm_free(ptr);
		}
	}

    return newptr;
}

static void mm_check() {
    printf("===========================\n");
    printf("[Log] mm_check start\n");
    // iterate through the seglist, print out the blocks in each of it
    for (int list = 0; list < NUM_SEG_LIST; ++list) {
        printf("[List #%d] %p : ", list, &seg_list[list]);

        if (!seg_list[list]) {
            printf("empty list\n");
            continue;
        }
        void *curr = seg_list[list];
        
        while(curr) {
            printf("[curr_address: %p, curr_allocated: %d, blk_size: %d, prev: %p, next: %p]", curr, GET_ISALLOCATED(HDRP(curr)), GET_SIZE(HDRP(curr)), GET_PREV_PTR(curr), GET_NEXT_PTR(curr));
            curr = GET_NEXT_PTR(curr);
            if (curr) {
                printf(" -> ");
            } else {
                printf("\n");
            }
        }
    }
    printf("===========================\n");
}













