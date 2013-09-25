#pragma once

#include <ngx_core.h>

typedef struct {
    int isInitialized;
    ngx_pool_t *pool;
    ngx_chain_t *startLink, *lastLink, *endLink;

    size_t newBufferSize, size, capacity, fragmentedCapacity;
} strb_t;

#define strbFree(strb) ((strb)->capacity - (strb)->size)

#define strbCurFree(strb)  ((size_t)((strb)->lastLink->buf->end - (strb)->lastLink->buf->last))
#define strbCurLast(strb)  ((strb)->lastLink->buf->last)
#define strbCurEnd(strb)   ((strb)->lastLink->buf->end)
#define strbCurPos(strb)   ((strb)->lastLink->buf->pos)
#define strbCurStart(strb) ((strb)->lastLink->buf->start)
#define strbCurBuf(strb)   ((strb)->lastLink->buf)
#define strbCurSize(strb)  ((size_t)((strb)->lastLink->buf->last - (strb)->lastLink->buf->start))

#define strbAppendNgxString(strb, str) strbAppendMemory(strb, (str)->data, (str)->len)
#define strbAppendCString(strb, str) strbAppendMemory(strb, (u_char*)str, strlen((char*)(str))) //TODO: Rewrite without strlen

#define strbUseNextChain(strb) (strb->lastLink = strb->lastLink->next)

#define strbDecreaseSizeBy(strb, decr) strbSetSize(strb, (strb)->size - (decr))

#define strbEnsureFreeCapacity(strb, capacityEnsurance) strbEnsureCapacity(strb, (strb)->size + (capacityEnsurance))

int strbInit(strb_t *strb, ngx_pool_t *pool, size_t newBufferSize, size_t capacity);
int strbEnsureCapacity(strb_t *strb, size_t capacityEnsurance);
int strbEnsureContinuousCapacity(strb_t *strb, size_t capacity);
int strbAppendMemory(strb_t *strb, u_char *src, size_t size);
int strbAppendSingle(strb_t *strb, u_char value);
int strbAppendPad(strb_t *strb, u_char c, size_t padLength);
int strbVFormat(strb_t *strb, const char *fmt, va_list args);
int strbFormat(strb_t *strb, const char *fmt, ...);
int strbSetSize(strb_t *strb, size_t size);
int strbTrim(strb_t *strb/*, int doInfixTrim*/);
int strbEscapeUri(strb_t *strb, u_char *src, size_t size, ngx_uint_t type);
int strbEscapeHtml(strb_t *strb, u_char *src, size_t size);