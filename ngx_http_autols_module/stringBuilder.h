#ifndef _STRING_BUILDER_INCLUDED_
#define _STRING_BUILDER_INCLUDED_

#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <memory.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>



typedef struct stringBuilderChainLink stringBuilderChainLink;
struct stringBuilderChainLink {
	char *start, *end, *last;
	stringBuilderChainLink *next, *prev;
};


typedef struct stringBuilder stringBuilder;

typedef stringBuilderChainLink* (*stringBuilderAppendChainLinks)(stringBuilder *strb, int32_t count, int32_t size);
typedef void(*stringBuilderDisposeChainLinks)(stringBuilder *strb);

typedef struct {
	int32_t newBufferSize;
	stringBuilderAppendChainLinks append;
	stringBuilderDisposeChainLinks dispose;
	void *config;
} stringBuilderAlloc;

struct stringBuilder {
	stringBuilderAlloc *alloc;
	int32_t isInitialized, size, capacity, totalCapacity;
	stringBuilderChainLink *startLink, *lastLink, *endLink;
};

#define strbFree(strb) ((strb)->capacity - (strb)->size)

#define strbCurEnd(strb)   ((strb)->lastLink->end)
#define strbCurLast(strb)  ((strb)->lastLink->last)
#define strbCurStart(strb) ((strb)->lastLink->start)
#define strbCurFree(strb)  (strbCurEnd(strb) - strbCurLast(strb))
#define strbCurSize(strb)  (strbCurLast(strb) - strbCurStart(strb))

#define strbAppendCString(strb, str) strbAppendMemory(strb, str, strlen(str)) //TODO: Rewrite without strlen

#define strbUseNextChain(strb) {strb->lastLink = strb->lastLink->next; }

#define strbDecreaseSizeBy(strb, decr) strbSetSize(strb, (strb)->size - (decr))

#define strbEnsureFreeCapacity(strb, capacityEnsurance) strbEnsureCapacity(strb, (strb)->size + (capacityEnsurance))

int32_t strbInit(stringBuilder *strb, int32_t minCapacity, stringBuilderAlloc *alloc);
int32_t strbDefaultInit(stringBuilder *strb, int32_t minCapacity, int32_t newBufferSize);
int32_t strbEnsureCapacity(stringBuilder *strb, int32_t capacity);
int32_t strbEnsureContinuousCapacity(stringBuilder *strb, int32_t capacity);
int32_t strbAppendMemory(stringBuilder *strb, char *src, int32_t size);
int32_t strbAppendStrb(stringBuilder *dst, stringBuilder *src);
int32_t strbAppendSingle(stringBuilder *strb, char value);
int32_t strbAppendRepeat(stringBuilder *strb, char c, int32_t length);
//int32_t strbVFormat(stringBuilder *strb, const char *fmt, va_list args);
int32_t strbFormat(stringBuilder *strb, const char *fmt, ...);
int32_t strbSetSize(stringBuilder *strb, int32_t size);
int32_t strbTrim(stringBuilder *strb/*, int32_t doInfixTrim*/);
int32_t strbEscapeUri(stringBuilder *strb, char *src, int32_t size, int32_t type);
int32_t strbEscapeHtml(stringBuilder *strb, char *src, int32_t size);
int32_t strbToCString(stringBuilder *strb, char *dst);

typedef int32_t(*strbTransform_fptr)(stringBuilder *strb, char *src, int32_t size, va_list args);
int32_t strbTransformStrb(stringBuilder *dst, stringBuilder *src, strbTransform_fptr strbTransformMethod, ...);
int32_t strbTransform(stringBuilder *strb, strbTransform_fptr strbTransformMethod, ...);

int32_t strbTransFormat(stringBuilder *strb, char *src, int32_t size, va_list args);
int32_t strbTransEscapeHtml(stringBuilder *strb, char *src, int32_t size, va_list args);
int32_t strbTransEscapeUri(stringBuilder *strb, char *src, int32_t size, va_list args);
int32_t strbTransPadLeft(stringBuilder *strb, char *src, int32_t size, va_list args);

int32_t stringBuilderAppendChainLinksCalloc(stringBuilder *strb, int32_t count, int32_t size);
void stringBuilderDisposeChainLinksCalloc(stringBuilder *strb);

void strbDispose(stringBuilder *strb);

#endif