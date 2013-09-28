#pragma once

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

ngx_log_t *connLog;

#define DEBUG_STRINGBUILDER 0

#if DEBUG_STRINGBUILDER
#define logDebugMsg0(log, fmt)                                                 ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0, fmt)
#define logDebugMsg1(log, fmt, arg1)                                           ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, fmt, arg1)
#define logDebugMsg2(log, fmt, arg1, arg2)                                     ngx_log_debug2(NGX_LOG_DEBUG_HTTP, log, 0, fmt, arg1, arg2)
#define logDebugMsg3(log, fmt, arg1, arg2, arg3)                               ngx_log_debug3(NGX_LOG_DEBUG_HTTP, log, 0, fmt, arg1, arg2, arg3)
#define logDebugMsg4(log, fmt, arg1, arg2, arg3, arg4)                         ngx_log_debug4(NGX_LOG_DEBUG_HTTP, log, 0, fmt, arg1, arg2, arg3, arg4)
#define logDebugMsg5(log, fmt, arg1, arg2, arg3, arg4, arg5)                   ngx_log_debug5(NGX_LOG_DEBUG_HTTP, log, 0, fmt, arg1, arg2, arg3, arg4, arg5)
#define logDebugMsg6(log, fmt, arg1, arg2, arg3, arg4, arg5, arg6)             ngx_log_debug6(NGX_LOG_DEBUG_HTTP, log, 0, fmt, arg1, arg2, arg3, arg4, arg5, arg6)
#define logDebugMsg7(log, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7)       ngx_log_debug7(NGX_LOG_DEBUG_HTTP, log, 0, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define logDebugMsg8(log, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) ngx_log_debug8(NGX_LOG_DEBUG_HTTP, log, 0, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#else
#define logDebugMsg0(log, fmt)                                                 
#define logDebugMsg1(log, fmt, arg1)                                           
#define logDebugMsg2(log, fmt, arg1, arg2)                                     
#define logDebugMsg3(log, fmt, arg1, arg2, arg3)                               
#define logDebugMsg4(log, fmt, arg1, arg2, arg3, arg4)                         
#define logDebugMsg5(log, fmt, arg1, arg2, arg3, arg4, arg5)                   
#define logDebugMsg6(log, fmt, arg1, arg2, arg3, arg4, arg5, arg6)             
#define logDebugMsg7(log, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7)       
#define logDebugMsg8(log, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) 
#endif



typedef struct {
    int isInitialized;
    ngx_pool_t *pool;
    ngx_chain_t *startLink, *lastLink, *endLink;

    int32_t newBufferSize, size, capacity;
} strb_t;

#define strbFree(strb) ((int32_t)((strb)->capacity - (strb)->size))

#define strbCurBuf(strb)   ((strb)->lastLink->buf)
#define strbCurEnd(strb)   ((strb)->lastLink->buf->end)
#define strbCurPos(strb)   ((strb)->lastLink->buf->pos)
#define strbCurLast(strb)  ((strb)->lastLink->buf->last)
#define strbCurStart(strb) ((strb)->lastLink->buf->start)
#define strbCurFree(strb)  ((int32_t)(strbCurEnd(strb) - strbCurLast(strb)))
#define strbCurSize(strb)  ((int32_t)(strbCurLast(strb) - strbCurStart(strb)))

#define strbAppendNgxString(strb, str) strbAppendMemory(strb, (str)->data, (str)->len)
#define strbAppendCString(strb, str) strbAppendMemory(strb, (u_char*)str, strlen((char*)str)) //TODO: Rewrite without strlen

#define strbUseNextChain(strb) { strb->lastLink->buf->last_in_chain = 0; strb->lastLink = strb->lastLink->next; strb->lastLink->buf->last_in_chain = 1; }

#define strbDecreaseSizeBy(strb, decr) strbSetSize(strb, (strb)->size - (decr))

#define strbEnsureFreeCapacity(strb, capacityEnsurance) strbEnsureCapacity(strb, (strb)->size + (capacityEnsurance))

int strbInit(strb_t *strb, ngx_pool_t *pool, int32_t newBufferSize, int32_t minCapacity);
int strbEnsureCapacity(strb_t *strb, int32_t capacity);
int strbEnsureContinuousCapacity(strb_t *strb, int32_t capacity);
int strbAppendMemory(strb_t *strb, u_char *src, int32_t size);
int strbAppendStrb(strb_t *dst, strb_t *src);
int strbAppendSingle(strb_t *strb, u_char value);
int strbAppendRepeat(strb_t *strb, u_char c, int32_t length);
int strbVFormat(strb_t *strb, const char *fmt, va_list args);
int strbFormat(strb_t *strb, const char *fmt, ...);
int strbSetSize(strb_t *strb, int32_t size);
int strbTrim(strb_t *strb/*, int doInfixTrim*/);
int strbEscapeUri(strb_t *strb, u_char *src, int32_t size, uint32_t type);
int strbEscapeHtml(strb_t *strb, u_char *src, int32_t size);
int strbToCString(strb_t *strb, u_char **data);

typedef int (*strbTransform_fptr)(strb_t *strb, u_char *src, int32_t size, va_list args);
int strbTransformStrb(strb_t *dst, strb_t *src, strbTransform_fptr strbTransformMethod, ...);

int strbTransFormat(strb_t *strb, u_char *src, int32_t size, va_list args);
int strbTransEscapeHtml(strb_t *strb, u_char *src, int32_t size, va_list args);
int strbTransEscapeUri(strb_t *strb, u_char *src, int32_t size, va_list args);
