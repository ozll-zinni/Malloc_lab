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
    "team 10",
    /* First member's full name */
    "Yejin Kim",
    /* First member's email address */
    "dpwls0454@naver.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


// 기본 상수, 매크로 
#define WSIZE 4             
#define DSIZE 8             
#define CHUNKSIZE (1 << 12) 

#define MAX(x, y) (x > y ? x : y)
#define PACK(size, alloc) (size | alloc)                              
#define GET(p) (*(unsigned int *)(p))                                 
#define PUT(p, val) (*(unsigned int *)(p) = (unsigned int)(val))      
#define GET_SIZE(p) (GET(p) & ~0x7)                                   

#define GET_ALLOC(p) (GET(p) & 0x1)                                   
#define HDRP(bp) ((char *)(bp)-WSIZE)                                 
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)          
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE))) 

#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))   

#define GET_SUCC(bp) (*(void **)((char *)(bp) + WSIZE)) 
#define GET_PRED(bp) (*(void **)(bp))                

static char *free_listp; 

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);


static void splice_free_block(void *bp); 
static void add_free_block(void *bp);    

int mm_init(void)
{
    // 초기 힙 생성
    if ((free_listp = mem_sbrk(8 * WSIZE)) == (void *)-1)
        return -1;
    PUT(free_listp, 0);                                
    PUT(free_listp + (1 * WSIZE), PACK(2 * WSIZE, 1)); 
    PUT(free_listp + (2 * WSIZE), PACK(2 * WSIZE, 1)); 
    PUT(free_listp + (3 * WSIZE), PACK(4 * WSIZE, 0)); 
    PUT(free_listp + (4 * WSIZE), NULL);               
    PUT(free_listp + (5 * WSIZE), NULL);               
    PUT(free_listp + (6 * WSIZE), PACK(4 * WSIZE, 0)); 
    PUT(free_listp + (7 * WSIZE), PACK(0, 1));         

    free_listp += (4 * WSIZE); 

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

void *mm_malloc(size_t size)
{
    size_t asize;     
    size_t extendsize; 
    char *bp;

    // 잘못된 요청 분기
    if (size == 0)
        return NULL;

    // 사이즈 조정 
    if (size <= DSIZE)     
        asize = 2 * DSIZE; 
    else
        asize = DSIZE * ((size + DSIZE + DSIZE - 1) / DSIZE); 


    // 가용 블록 검색
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize); 
        return bp;       
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}


/*
 * mm_free - Freeing a block does nothing.
 */

void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}


void *mm_realloc(void *ptr, size_t size)
{
    // 예외 처리
    if (ptr == NULL)
        return mm_malloc(size);

    if (size <= 0) 
    {
        mm_free(ptr);
        return 0;
    }

    // 새 블록에 할당 
    void *newptr = mm_malloc(size); 
    if (newptr == NULL)
        return NULL;

    /* 데이터 복사 */
    size_t copySize = GET_SIZE(HDRP(ptr)) - DSIZE; 
    if (size < copySize)                           
        copySize = size;                         

    memcpy(newptr, ptr, copySize);


    mm_free(ptr);
    
    return newptr;
}

static void *extend_heap(size_t words)
{
    char *bp;


    void *newptr = mm_malloc(size); 
    if (newptr == NULL)
        return NULL; 
    
    size_t copySize = GET_SIZE(HDRP(ptr)) - DSIZE; 
    if (size < copySize)                           
        copySize = size;                          


    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; 

    if ((long)(bp = mem_sbrk(size)) == -1) 
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));        
    PUT(FTRP(bp), PACK(size, 0));       
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); 

    return coalesce(bp); 
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); 
    size_t size = GET_SIZE(HDRP(bp));                 

    if (prev_alloc && next_alloc) 
    {
        add_free_block(bp);
        return bp;      
    }
    else if (prev_alloc && !next_alloc)
    {
        splice_free_block(NEXT_BLKP(bp)); 
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0)); 
        PUT(FTRP(bp), PACK(size, 0)); 
    }
    else if (!prev_alloc && next_alloc)
    {
        splice_free_block(PREV_BLKP(bp)); 
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); 
        PUT(FTRP(bp), PACK(size, 0));            
        bp = PREV_BLKP(bp);                      
    }
    else
    {
        splice_free_block(PREV_BLKP(bp)); 
        splice_free_block(NEXT_BLKP(bp)); 
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); 
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); 
        bp = PREV_BLKP(bp);                
    }
    add_free_block(bp); 
    return bp;         
}

static void *find_fit(size_t asize)
{
    void *bp = free_listp;
    while (bp != NULL) 
    {
        if ((asize <= GET_SIZE(HDRP(bp))))
            return bp;
        bp = GET_SUCC(bp); 
    }
    return NULL;
}

static void place(void *bp, size_t asize)
{
    splice_free_block(bp); 

    size_t csize = GET_SIZE(HDRP(bp)); 

    if ((csize - asize) >= (2 * DSIZE)) 
    {
        PUT(HDRP(bp), PACK(asize, 1)); 
        PUT(FTRP(bp), PACK(asize, 1));

        PUT(HDRP(NEXT_BLKP(bp)), PACK((csize - asize), 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK((csize - asize), 0));
        add_free_block(NEXT_BLKP(bp)); 
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1)); 
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

static void splice_free_block(void *bp)
{
    if (bp == free_listp)
    {
        free_listp = GET_SUCC(free_listp); 
        return;
    }
    
    GET_SUCC(GET_PRED(bp)) = GET_SUCC(bp);
    
    if (GET_SUCC(bp) != NULL)
        GET_PRED(GET_SUCC(bp)) = GET_PRED(bp);
}

static void add_free_block(void *bp)
{
    GET_SUCC(bp) = free_listp;    
    if (free_listp != NULL)       
        GET_PRED(free_listp) = bp; 
    free_listp = bp;       
}        
