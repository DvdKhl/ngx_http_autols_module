#pragma once

#include <ngx_core.h>

typedef struct {
    ngx_pool_t *pool;
    ngx_chain_t *firstLink, *currentLink, *lastLink;

    size_t newBufferSize, size, capacity, fragmentedCapacity;
} strb_t;

#define strbFree(strb) ((strb)->capacity - (strb)->size)

#define strbCurFree(strb) ((strb)->currentLink->buf->end - (strb)->currentLink->buf->last)
#define strbCurLast(strb) ((strb)->currentLink->buf->last)
#define strbCurEnd(strb) ((strb)->currentLink->buf->end)
#define strbCurPos(strb) ((strb)->currentLink->buf->pos)
#define strbCurBuf(strb) ((strb)->currentLink->buf)
#define strbCurSize(strb) ((strb)->currentLink->buf->end - (strb)->currentLink->buf->last)

#define strbAppendNgxString(strb, str) (strbAppendMemory(strb, (str)->data, (str)->len))
#define strbAppendCString(strb, str) (strbAppendMemory(strb, str, strlen((char*)(str)))) //TODO: Rewrite without strlen

#define strbDecreaseSizeBy(strb, decr) (strbSetSize(strb, (strb)->size - (decr)))

int strbInit(strb_t *strb, ngx_pool_t *pool, size_t newBufferSize, size_t capacity);
int strbEnsureCapacity(strb_t *strb, size_t capacityEnsurance);
int strbAppendMemory(strb_t *strb, u_char *src, size_t size);
int strbAppendSingle(strb_t *strb, u_char value);
int strbVFormat(strb_t *strb, const char *fmt, va_list args);
int strbFormat(strb_t *strb, const char *fmt, ...);
int strbSetSize(strb_t *strb, size_t size);
int strbTrim(strb_t *strb/*, int doInfixTrim*/);