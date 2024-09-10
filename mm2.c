#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    "team 10",
    "Yejin Kim",
    "dpwls0454@naver.com",
    "",
    ""};

#define ALIGNMENT 8

#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

/* 기본 상수 */
#define WSIZE 4             
#define DSIZE 8             
#define CHUNKSIZE (1 << 12) 

/* 힙에 접근/순회하는 데 사용할 매크로 */
#define MAX(x, y) (x > y ? x : y)
#define PACK(size, alloc) (size | alloc)                              
#define GET(p) (*(unsigned int *)(p))                                 
#define PUT(p, val) (*(unsigned int *)(p) = (val))                    
#define GET_SIZE(p) (GET(p) & ~0x7)                                  
#define GET_ALLOC(p) (GET(p) & 0x1)                                   
#define HDRP(bp) ((char *)(bp)-WSIZE)                                 
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)          
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE))) 
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))   

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

int mm_init(void)
{
    // 초기 힙 생성
    char *heap_listp;
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) 
        return -1;
    PUT(heap_listp, 0);                            
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));    

    // 힙을 CHUNKSIZE bytes로 확장
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

    /* 사이즈 조정 */
    if (size <= DSIZE)     
        asize = 2 * DSIZE; 
    else
        asize = DSIZE * ((size + DSIZE + DSIZE - 1) / DSIZE); 

    /* 가용 블록 검색 */
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize); 
        return bp;        
    }

    /* 적합한 블록이 없을 경우 힙확장 */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

// 기존에 할당된 메모리 블록의 크기 변경
// `기존 메모리 블록의 포인터`, `새로운 크기`
void *mm_realloc(void *ptr, size_t size)
{
    /* 예외 처리 */
    if (ptr == NULL) 
        return mm_malloc(size);

    if (size <= 0) 
    {
        mm_free(ptr);
        return 0;
    }

    /* 새 블록에 할당 */
    void *newptr = mm_malloc(size); 
    if (newptr == NULL)
        return NULL; 

    /* 데이터 복사 */
    size_t copySize = GET_SIZE(HDRP(ptr)) - DSIZE; 
    if (size < copySize)                           
        copySize = size;                          

    memcpy(newptr, ptr, copySize); 

    /* 기존 블록 반환 */
    mm_free(ptr);

    return newptr;
}

static void *extend_heap(size_t words)
{
    char *bp;

    // 더블 워드 정렬 유지
    // size: 확장할 크기
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
        return bp;

    else if (prev_alloc && !next_alloc) 
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0)); 
        PUT(FTRP(bp), PACK(size, 0)); 
    }
    else if (!prev_alloc && next_alloc) 
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); 
        PUT(FTRP(bp), PACK(size, 0));            
        bp = PREV_BLKP(bp);                      
    }
    else 
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); 
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); 
        bp = PREV_BLKP(bp);                      
    }

    return bp;
}

static void *find_fit(size_t asize)
{
    void *bp = mem_heap_lo() + 2 * WSIZE; 
    while (GET_SIZE(HDRP(bp)) > 0)
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) 
            return bp;                                           
        bp = NEXT_BLKP(bp);                                    
    }
    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp)); 

    if ((csize - asize) >= (2 * DSIZE)) 
    {
        PUT(HDRP(bp), PACK(asize, 1)); 
        PUT(FTRP(bp), PACK(asize, 1));

        PUT(HDRP(NEXT_BLKP(bp)), PACK((csize - asize), 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK((csize - asize), 0));
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1)); 
        PUT(FTRP(bp), PACK(csize, 1));
    }
}