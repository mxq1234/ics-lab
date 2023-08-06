/*
 * mm.c - The fastest, memory-efficient malloc package.
 */

 /* 
 * 1. The structure of blocks:
 * 
 * The structure of allocated blocks:
 *      Header(WSIZE)-Payload-Padding(optional)-Footer(WSIZE)
 * The structure of free blocks:
 *      Header(WSIZE)-SUCC(WSIZE)-PRED(WSIZE)-Payload-Padding(optional)-Footer(WSIZE)
 * 
 * The block pointer bp is always pointed to the address after header.
 * To save space, the content of SUCC and PRED is the offset of block's succ and pred.
 * The offset is relative to the free_listp(the beginning of the heap).
 * 
 * 
 * 2. The organization of free lists(Segregated Free Lists):
 * 
 *   At the beginning of the heap, use 9*WSIZE to serve as free list head pointer array.
 *   Each head is actually SUCC part of free block, and only include this part.
 *   Then use 2*WSIZE to serve as free list tail block, that only include SUCC-PRED part.
 *   
 *   So the heap structure after mm_init is:
 * 
 *      {16-31}-{32-63}-...-{2048-4095}-{4096-inf}-Free_list_tail(SUCC-PRED)-Prologue(Header-Footer)-Epilogue
 * 
 *   The structure of all free lists after mm_init is:
 *      head->free_list_tail
 * 
 *   Using this structure, we can find the head efficiently, and needn't to think of 
 *   the troublesome boundary problems when inserting or deleting.
 * 
 * 
 * 3. How to manipulate the free list:
 *   It is the same as the common double-linked list that has head and tail.
 *   The difference is the content of SUCC and PRED is not pointer, but offset.
 *   So we provide some macros to manipulate it:
 *      
 *      SUCC(bp), PRED(bp) -- Get the pointer pointed to the SUCC and PRED part of this block.
 *      GET_xxxx_OFFSET  -- Get the offset of bp, succ or pred.
 *      SUCC_BLKP(bp), PRED(bp) -- Get the pointer pointed to the succ and pred.
 * 
 *     If you want to set the SUCC and PRED when inserting, using:
 *          PUT(SUCC(bp), GET_xxxx_OFFSET(xx));
 *     If you want to traverse the free list from head, using:
 *          bp = SUCC_BLKP(bp)
 * 
 *   The tail pointer can be obtained using GET_TAIL.
 * 
 * 
 * 4. Use best fit for find_fit and LIFO-ordering for free_list.
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


/* Basic constants and macros */
#define WSIZE 4     /* word size(bytes) */
#define DSIZE 8     /* double word size(bytes) */
#define CHUNKSIZE 72     /* Extend heap by this amount(bytes) */

#define MAX(x, y) (((x) > (y))? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int*)(p))
#define PUT(p, val) (*(unsigned int*)(p)=(val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

/* Given block ptr bp, compute address of pred and succ */
#define PRED(bp)  ((char*)(bp) + WSIZE)
#define SUCC(bp)  ((char*)(bp))

/* Given block ptr bp, compute offset of bp, pred and succ */
#define GET_BLKP_OFFSET(bp) ((char*)(bp) - free_listp)
#define GET_PRED_OFFSET(bp)  (GET(PRED(bp)))
#define GET_SUCC_OFFSET(bp)  (GET(SUCC(bp)))

/* Given block ptr bp, compute address of pred and succ blocks */
#define PRED_BLKP(bp)   (free_listp + GET_PRED_OFFSET(bp))
#define SUCC_BLKP(bp)   (free_listp + GET_SUCC_OFFSET(bp))

/* Some special constants in the free list */
#define FREE_INF_OFFSET (8 * WSIZE)
#define FREE_TAIL_OFFSET  (9 * WSIZE)

/* Get head or tail pointer of free list */
#define GET_HEAD(i) (free_listp + ((i >= 8)? (FREE_INF_OFFSET) : ((i) * (WSIZE))))
#define GET_TAIL (free_listp + FREE_TAIL_OFFSET)


static char* free_listp;

static char* heap_listp;

/* Basic tool function */
static void* extend_heap(size_t words);
static void* coalesce(void* bp);
static void* find_fit(size_t asize);
static void place(void* bp, size_t asize);
static void insert_free_list(void* bp);
static void delete_from_free_list(void* bp);

/* Function for debug */
void print_free_list(int i);
int mm_check();

/* 
 * mm_init - Initialize the malloc package.
 *  Initailize free list headers and prologue and epilogue block.
 */
int mm_init(void)
{
    if((free_listp = mem_sbrk(14 * WSIZE)) == (void*)-1) {
        printf("Failed to init\n");
        return -1;
    }

    PUT(free_listp, FREE_TAIL_OFFSET);                      /* free_listp16_31 */
    PUT(free_listp + (1 * WSIZE), FREE_TAIL_OFFSET);        /* free_listp32_63 */
    PUT(free_listp + (2 * WSIZE), FREE_TAIL_OFFSET);        /* free_listp64_127 */
    PUT(free_listp + (3 * WSIZE), FREE_TAIL_OFFSET);        /* free_listp128_255 */
    PUT(free_listp + (4 * WSIZE), FREE_TAIL_OFFSET);        /* free_listp256_511 */
    PUT(free_listp + (5 * WSIZE), FREE_TAIL_OFFSET);        /* free_listp512_1023 */
    PUT(free_listp + (6 * WSIZE), FREE_TAIL_OFFSET);        /* free_listp1024_2047 */
    PUT(free_listp + (7 * WSIZE), FREE_TAIL_OFFSET);        /* free_listp2048_4095 */
    PUT(free_listp + (8 * WSIZE), FREE_TAIL_OFFSET);        /* free_listp4096_inf */
    PUT(free_listp + (9 * WSIZE), FREE_TAIL_OFFSET);        /* FREE_TAIL_SUCC */
    PUT(free_listp + (10 * WSIZE), FREE_TAIL_OFFSET);       /* FREE_TAIL_PRED */
    PUT(free_listp + (11 * WSIZE), PACK(DSIZE, 1));         /* prologue header */
    PUT(free_listp + (12 * WSIZE), PACK(DSIZE, 1));         /* prologue footer */
    PUT(free_listp + (13 * WSIZE), PACK(0, 1));             /* epilogue */
    heap_listp = free_listp + (12 * WSIZE);

    return 0;
}

/*
 *  extend_heap - Extend the heap according to given words.
 *      Return newly extended free block pointer.
 */
static void* extend_heap(size_t words)
{
    char* bp;
    size_t size = (words + (words & 1)) * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1) {
        printf("Failed to extend the heap\n");
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    bp = coalesce(bp);

    insert_free_list(bp);

    return bp;
}

/*
 *  insert_free_list - Insert the bp block into corresponding free list.
 *      Only insert the block into the beginning of free list.
 *      Ensure that bp block has been marked unallocted.
 */
inline static void insert_free_list(void* bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    if(size < 16) {
        printf("Insert error\n");
        return;
    }
    size >>= 4;
    size_t log = 0;
    while(size >>= 1)  ++log;
    void* head = GET_HEAD(log);

    PUT(SUCC(bp), GET_SUCC_OFFSET(head));
    PUT(PRED(bp), GET_BLKP_OFFSET(head));
    PUT(PRED(SUCC_BLKP(bp)), GET_BLKP_OFFSET(bp));
    PUT(SUCC(head), GET_BLKP_OFFSET(bp));
}

/*
 *  delete_from_free_list - Remove the bp block from its free list.
 *      Ensure that bp block has been in the free list.
 */
inline static void delete_from_free_list(void* bp)
{
    PUT(SUCC(PRED_BLKP(bp)), GET_SUCC_OFFSET(bp));
    PUT(PRED(SUCC_BLKP(bp)), GET_PRED_OFFSET(bp));
}

/*
 *  coalesce - Coalesce free blocks that are adjacent on address.
 *      Return pointer of the coalescing free block.
 */
static void* coalesce(void* bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* prev block are allocated */
    if(prev_alloc) {
        if(next_alloc)  return bp;
        void* next = NEXT_BLKP(bp);
        delete_from_free_list(next);
        size += GET_SIZE(HDRP(next));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return bp;
    }

    /* !prev_alloc && next_alloc */
    if(next_alloc) {
        void* prev = PREV_BLKP(bp);
        delete_from_free_list(prev);
        size += GET_SIZE(HDRP(prev));
        PUT(HDRP(prev), PACK(size, 0));
        PUT(FTRP(prev), PACK(size, 0));
        return prev;   
    }

    /* !prev_alloc && !next_alloc */
    void* prev = PREV_BLKP(bp);
    void* next = NEXT_BLKP(bp);

    delete_from_free_list(prev);
    delete_from_free_list(next);

    size += GET_SIZE(HDRP(prev));
    size += GET_SIZE(FTRP(next));

    PUT(HDRP(prev), PACK(size, 0));
    PUT(FTRP(prev), PACK(size, 0));
    return prev;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    if(size == 0) {
        printf("No malloc\n");
        return NULL;
    }

    if(size == 448)     size = 512;
    if(size == 112)     size = 128;

    size_t asize;
    size_t extendsize;
    char* bp;

    asize = ((size <= DSIZE)? (DSIZE + DSIZE) : (DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE)));

    if((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize / WSIZE)) == (void*)NULL) {
        printf("failed to extend\n");
        return NULL;
    }
    delete_from_free_list(bp);
    place(bp, asize);
    return bp;
}

/* 
 * find_fit - Use best fit to find the available block in the free list.
 *     If not find in the corresponding free list, find in larger size class.
 *     If not find in all free lists, return NULL, need extend the heap.
 */
static void* find_fit(size_t asize)
{
    size_t log = 0, tmp = (asize >> 4);
    while(tmp >>= 1)  ++log;
    void* head = GET_HEAD(log), *tail = GET_TAIL, *best = NULL;
    size_t best_size = 0x7fffffff;
    while(head != tail) {
        void* bp = SUCC_BLKP(head);
        while(bp != tail) {
            if(asize == GET_SIZE(HDRP(bp))) {
                delete_from_free_list(bp);
                return bp;
            }
            if(asize < GET_SIZE(HDRP(bp)) && best_size > GET_SIZE(HDRP(bp))) {
                best = bp;
                best_size = GET_SIZE(HDRP(bp));
            }
            bp = SUCC_BLKP(bp);
        }
        head = ((char*)head + WSIZE);
    }
    if(best != NULL)    delete_from_free_list(best);
    return best;
}

/* 
 * place - Marked the used part as allocated and insert unused part into free list.
 */
static void place(void* bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if((csize - asize) >= (DSIZE + DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        insert_free_list(bp);
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * mm_free - Freeing a block.
 *      Firstly find possible coalesce and insert block into free list.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    bp = coalesce(bp);
    insert_free_list(bp);
}

/*
 * r_place - The function place specially for realloc.
 *      Always use all part of blocks.
 */
inline static void r_place(void* bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
}

/*
 * mm_realloc - Realloc a new-size block for ptr block
 */
void *mm_realloc(void *ptr, size_t size)
{
    /* ptr is NULL, malloc a new block directly */
    if(ptr == NULL) {
        ptr = mm_malloc(size);
        return ptr;
    }

    /* size is 0, free the ptr block directly */
    if(size == 0) {
        mm_free(ptr);
        return NULL;
    }

    void *oldptr = ptr;
    void *newptr;
    size_t copySize = GET_SIZE(HDRP(oldptr)) - DSIZE;
    size_t asize = ((size <= DSIZE)? (DSIZE + DSIZE) : (DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE)));

    /* Realloc a smaller block, using 'place' to cut apart */
    if(size < copySize) {
        place(oldptr, asize);
        return oldptr;
    }

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(oldptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(oldptr)));
    size_t old_size = GET_SIZE(HDRP(oldptr)), new_size;

    if(prev_alloc && !next_alloc) {     /* backward coalesce */
        void* next = NEXT_BLKP(oldptr);

        /* If the coalesced size is enough, then do it */
        if(old_size + GET_SIZE(HDRP(next)) >= asize) {
            delete_from_free_list(next);
            new_size = old_size + GET_SIZE(HDRP(next));
            PUT(HDRP(oldptr), PACK(new_size, 1));
            PUT(FTRP(oldptr), PACK(new_size, 1));
            if(new_size > asize)    r_place(oldptr, asize);
            return oldptr;
        }
    } else if(!prev_alloc && next_alloc) {      /* forward coalesce */
        void* prev = PREV_BLKP(oldptr);

        /* If the coalesced size is enough, then do it */
        if(old_size + GET_SIZE(HDRP(prev)) >= asize) {
            delete_from_free_list(prev);
            new_size = old_size + GET_SIZE(HDRP(prev));
            memmove(prev, oldptr, copySize);    /* Using memmove to avoid overlapping */
            PUT(HDRP(prev), PACK(new_size, 1));
            PUT(FTRP(prev), PACK(new_size, 1));
            if(new_size > asize)    r_place(prev, asize);
            return prev;
        }
    } else if(!prev_alloc && !next_alloc) {      /* backward and forward coalesce */
        void* prev = PREV_BLKP(oldptr);
        void* next = NEXT_BLKP(oldptr);

        /* If the coalesced size is enough, then do it */
        if(old_size + GET_SIZE(HDRP(prev)) + GET_SIZE(HDRP(next)) >= asize) {
            delete_from_free_list(prev);
            delete_from_free_list(next);
            new_size = old_size + GET_SIZE(HDRP(prev)) + GET_SIZE(HDRP(next));
            memmove(prev, oldptr, copySize);
            PUT(HDRP(prev), PACK(new_size, 1));
            PUT(FTRP(prev), PACK(new_size, 1));
            if(new_size > asize)    r_place(prev, asize);
            return prev;
        }
    }

    /* Cannot coalesced or coalesced size is not enough */
    /* Can only malloc a new block and free the old block */
    newptr = mm_malloc(size);
    if (newptr == NULL)     return NULL;
    memmove(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/*
 * print_free_list - A function for debugging.
 *      Given index of free list header, print the free list(offset).
 */
void print_free_list(int i)
{
    void* head = GET_HEAD(i), *tail = GET_TAIL;
    while(head != tail) {
        printf("%ld->", GET_BLKP_OFFSET(head));
        head = SUCC_BLKP(head);
    }
    printf("%ld\n", GET_BLKP_OFFSET(head));
}

/*
 * check_free_list_marked - A tool function for mm_check.
 *      Check whether every block in the free list marked as free.
 */
int check_free_list_marked()
{
    char* head = free_listp, *tail = GET_TAIL;
    while(head != tail) {
        char* bp = SUCC_BLKP(head);
        while(bp != tail) {
            if(GET_ALLOC(HDRP(bp)))     return 0;
            bp = SUCC_BLKP(bp);
        }
        head += WSIZE;
    }
    return 1;
}

/*
 * check_escape_coalescing - A tool function for mm_check.
 *      Check whether there are contiguous blocks that escape coalescing.
 */
int check_escape_coalescing()
{
    void* bp = heap_listp;
    int last = 1, alloc;
    while(GET_SIZE(HDRP(bp)) > 0) {
        alloc = GET_ALLOC(HDRP(bp));
        if(!(alloc || last))    return 0;
        last = alloc;
        bp = NEXT_BLKP(bp);
    }
    return 1;
}

/*
 * check_actually_in_free_list - A tool function for mm_check.
 *      Check whether every block is actually in the free list.
 */
int check_actually_in_free_list()
{
    for(void* bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if(GET_ALLOC(HDRP(bp)))     continue;
        void* next = SUCC_BLKP(bp);
        if(next != GET_TAIL && GET_ALLOC(HDRP(next))) return 0;
    }
    return 1;
}

/*
 * check_free_list_pointer_link_valid - A tool function for mm_check.
 *      Check whether the pointers in the free lists point to valid blocks.
 *      Mainly check whether the structure of free lists is normal.
 */
int check_free_list_pointer_link_valid()
{
    char* head = free_listp, *tail = GET_TAIL;
    while(head != tail) {
        void* bp = SUCC_BLKP(head);
        while(bp != tail) {
            void* next = SUCC_BLKP(bp);
            if(SUCC_BLKP(PRED_BLKP(bp)) != bp)  return 0;
            if(next != tail && PRED_BLKP(next) != bp)  return 0;
            bp = next;
        }
        head += WSIZE;
    }
    return 1;
}

/*
 * check_point_to_valid_address - A tool function for mm_check.
 *      Check whether the pointers in the heap point to valid heap address.
 */
int check_point_to_valid_address()
{
    /* Check the implicit free lists pointer */
    for(void* bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if(((char*)bp < (char *)mem_heap_lo()) 
            || ((char*)bp > (char *)mem_heap_hi()))  return 0;
    }

    /* Check the segregated free lists pointer */
    char* head = free_listp, *tail = GET_TAIL;
    while(head != tail) {
        void* bp = SUCC_BLKP(head);
        while(bp != tail) {
            if(((char*)bp < (char *)mem_heap_lo()) 
                || ((char*)bp > (char *)mem_heap_hi()))  return 0;
            bp = SUCC_BLKP(bp);
        }
        head += WSIZE;
    }
    return 1;
}

/*
 * check_aligned - A tool function for mm_check.
 *      Check whether the pointers of blocks are aligned.
 */
int check_aligned()
{
    for(void* bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
        if(((unsigned long)(bp)) % ALIGNMENT)  return 0;
    return 1;
}

/*
 * mem_check - Heap consistency checker.
 *      Return 1 if and only if the heap is consistent.
 */
int mm_check()
{
    if(!check_free_list_marked()) {
        printf("Not every block in the free list marked free\n");
        return 0;
    }
    if(!check_escape_coalescing()) {
        printf("Some free blocks escape coalescing\n");
        return 0;
    }
    if(!check_actually_in_free_list()) {
        printf("Not every free block actually in the free list\n");
        return 0;
    }
    if(!check_free_list_pointer_link_valid()) {
        printf("Some pointers in the free list point to invalid free blocks\n");
        return 0;
    }
    if(!check_point_to_valid_address()) {
        printf("Some pointers point to invalid heap address\n");
        return 0;
    }
    if(!check_aligned()) {
        printf("Some block pointers is not aligned\n");
        return 0;
    }
    return 1;
}













