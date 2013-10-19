#include "stringBuilder.h"

#if STRINGBUILDER_DEBUG
void(*strbLog)(char *format, ...);

#if NGX_HAVE_C99_VARIADIC_MACROS || _MSC_VER
#define strbLog(format, ...) strbLog(format, __VA_ARGS__)
#elif (NGX_HAVE_GCC_VARIADIC_MACROS)
#define strbLog(format, args...) strbLog(format, args)
#endif

#else
#define strbLog(params)
#endif

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

int32_t stringBuilderAppendChainLinksCalloc(stringBuilder *strb, int32_t count, int32_t size) {
	stringBuilderChainLink *prevLink, *link, *firstLink;

	int32_t i;
	prevLink = link = firstLink = NULL;
	for(i = 0; i < count; i++) {
		void *block = malloc(size + sizeof(stringBuilderChainLink));
		if(block == NULL) return 0;

		strbLog("strb: stringBuilderAppendChainLinksCalloc block=%d", block);

		link = (stringBuilderChainLink*)block;
		link->start = link->last = (char*)block + sizeof(stringBuilderChainLink);
		link->end = link->start + size;

		link->next = link->prev = NULL;
		if(prevLink != NULL) {
			link->prev = prevLink;
			prevLink->next = link;
		}

		if(firstLink == NULL) firstLink = link;
		prevLink = link;
	}

	if(strb->startLink == NULL) {
		strb->startLink = strb->lastLink = firstLink;
	} else {
		strb->endLink->next = firstLink;
		firstLink->prev = strb->endLink;
	}
	strb->endLink = link;

	strb->capacity += count * size;
	strb->totalCapacity += count * size;

	return 1;
}

void stringBuilderDisposeChainLinksCalloc(stringBuilder *strb) {
	stringBuilderChainLink *nextLink, *link = strb->startLink;

	do {
		strbLog("strb: stringBuilderDisposeChainLinksCalloc block=%d", link);
		nextLink = link->next;
		strbLog("strb: 1");
		free(link);
		strbLog("strb: 2");
	} while((link = nextLink) != NULL);
	strbLog("strb: 3");

	if(strb->alloc->config) {
		strbLog("strb: stringBuilderDisposeChainLinksCalloc strb->alloc->config=%d", strb->alloc->config);
		free(strb->alloc->config);
	}
	strbLog("strb: 4");
}


int32_t strbEnsureCapacity(stringBuilder *strb, int32_t capacity) {
	strbLog("strb: EnsureCapacity: strb(size=%d, capacity=%d, start=%d, last=%d, end=%d), capacity=%d", strb->size, strb->capacity, strb->startLink, strb->lastLink, strb->endLink, capacity);
	int32_t neededCapacity = capacity - strb->capacity;
	if(neededCapacity < 1) return 1;

	int32_t count = neededCapacity / strb->alloc->newBufferSize + ((neededCapacity % strb->alloc->newBufferSize) != 0);

	if(!strb->alloc->append(strb, count, strb->alloc->newBufferSize)) return 0;

	strbLog("strb: /EnsureCapacity: strb(size=%d, capacity=%d, start=%d, last=%d, end=%d)", strb->size, strb->capacity, strb->startLink, strb->lastLink, strb->endLink);
	return 1;
}

int32_t strbInit(stringBuilder *strb, int32_t minCapacity, stringBuilderAlloc *alloc) {
	if(minCapacity < 0 || alloc->newBufferSize <= 0 || strb == NULL || alloc->append == NULL) return 0;
	if(minCapacity == 0) minCapacity = 1;

	strb->alloc = alloc;
	strb->isInitialized = 1;
	strb->size = strb->capacity = strb->totalCapacity = 0;
	strb->startLink = strb->lastLink = strb->endLink = NULL;

	strbEnsureCapacity(strb, minCapacity);

	return 1;
}

int32_t strbDefaultInit(stringBuilder *strb, int32_t minCapacity, int32_t newBufferSize) {
	strbLog("strb: strbDefaultInit: strb(size=%d, capacity=%d, start=%d, last=%d, end=%d), minCapacity=%d, newBufferSize=%d", strb->size, strb->capacity, strb->startLink, strb->lastLink, strb->endLink, minCapacity, newBufferSize);
	if(minCapacity < 0 || newBufferSize <= 0 || strb == NULL) return 0;
	if(minCapacity == 0) minCapacity = 1;

	strb->alloc = (stringBuilderAlloc*)malloc(sizeof(stringBuilderAlloc));
	strb->alloc->config = strb->alloc;

	strb->alloc->newBufferSize = newBufferSize;
	strb->alloc->append = (stringBuilderAppendChainLinks)stringBuilderAppendChainLinksCalloc;
	strb->alloc->dispose = stringBuilderDisposeChainLinksCalloc;

	strb->isInitialized = 1;
	strb->size = strb->capacity = strb->totalCapacity = 0;
	strb->startLink = strb->lastLink = strb->endLink = NULL;

	strbEnsureCapacity(strb, minCapacity);
	strbLog("strb: /strbDefaultInit: strb(size=%d, capacity=%d, start=%d, last=%d, end=%d)", strb->size, strb->capacity, strb->startLink, strb->lastLink, strb->endLink);

	return 1;
}


int32_t strbEnsureContinuousCapacity(stringBuilder *strb, int32_t capacity) {
	strbLog("strb: EnsureContinuousCapacity: strb(size=%d, capacity=%d, start=%d, last=%d, end=%d)", strb->size, strb->capacity, strb->startLink, strb->lastLink, strb->endLink);
	if(capacity < 1 || strbCurFree(strb) >= capacity) return 1;

	int32_t appendedCapacity = max(capacity, strb->alloc->newBufferSize);
	if(!strb->alloc->append(strb, 1, appendedCapacity)) return 0;

	strbUseNextChain(strb);
	strbLog("strb: /EnsureContinuousCapacity: strb(size=%d, capacity=%d, start=%d, last=%d, end=%d)", strb->size, strb->capacity, strb->startLink, strb->lastLink, strb->endLink);

	return 1;
}

int32_t strbSetSize(stringBuilder *strb, int32_t size) {
	strbLog("strb: strbSetSize: strb(size=%d, capacity=%d, start=%d, last=%d, end=%d)", strb->size, strb->capacity, strb->startLink, strb->lastLink, strb->endLink);
	if(size < 0) return 0;

	if(strb->size < size) {
		if(!strbEnsureCapacity(strb, size)) return 0;

		size = size - strb->size;
		while(size) {
			int32_t toSet = min(strbCurFree(strb), size);
			//memzero(strbCurLast(strb), size);
			strbCurLast(strb) += toSet;

			strb->size += toSet;
			size -= toSet;

			if(size != 0) strbUseNextChain(strb);
		}

	} else {
		if(strbCurSize(strb) >= strb->size - size) {
			//Current chain is the one we're looking for
			strbCurLast(strb) -= strb->size - size;

		} else {
			int32_t sizeDiff = strb->size - size;
			while(sizeDiff != 0) {
				int32_t toRemove = min(strbCurSize(strb), sizeDiff);
				strbCurLast(strb) -= toRemove;
				sizeDiff -= toRemove;

				//if(strbCurSize(strb) == 0 && sizeDiff != 0) {
				if(sizeDiff != 0) {
					strb->lastLink = strb->lastLink->prev;
					strb->capacity += strbCurFree(strb);
				}
			}
		}
		strb->size = size;
	}
	strbLog("strb: /strbSetSize: strb(size=%d, capacity=%d, start=%d, last=%d, end=%d)", strb->size, strb->capacity, strb->startLink, strb->lastLink, strb->endLink);
	return 1;
}

int32_t strbAppendMemory(stringBuilder *strb, char *src, int32_t size) {
	strbLog("strb: strbAppendMemory: strb(size=%d, capacity=%d, start=%d, last=%d, end=%d), size=%d", strb->size, strb->capacity, strb->startLink, strb->lastLink, strb->endLink, size);
	if(size < 0) return 0;
	if(!strbEnsureFreeCapacity(strb, size)) return 0;

	while(size != 0) {
		int32_t copyLength = min(strbCurFree(strb), size);

		strbCurLast(strb) = (char*)memcpy(strbCurLast(strb), src, copyLength) + copyLength;
		strb->size += copyLength;
		size -= copyLength;
		src += copyLength;

		if(size != 0) strbUseNextChain(strb);
	}
	strbLog("strb: /strbAppendMemory: strb(size=%d, capacity=%d, start=%d, last=%d, end=%d)", strb->size, strb->capacity, strb->startLink, strb->lastLink, strb->endLink);
	return 1;
}

int32_t strbAppendSingle(stringBuilder *strb, char value) {
	if(!strbEnsureFreeCapacity(strb, 1)) return 0;
	if(strbCurFree(strb) == 0) strbUseNextChain(strb);

	*(strbCurLast(strb)++) = value;
	strb->size++;

	return 1;
}

int32_t strbAppendRepeat(stringBuilder *strb, char c, int32_t length) {
	if(!strbEnsureFreeCapacity(strb, length)) return 0;

	strb->size += length;
	while(length != 0) {
		int32_t toPad = min(length, strbCurFree(strb));
		memset(strbCurLast(strb), c, toPad);
		strbCurLast(strb) += toPad;
		length -= toPad;

		if(length != 0) strbUseNextChain(strb);
	}

	return 1;
}

int32_t strbAppendStrb(stringBuilder *dst, stringBuilder *src) {
	stringBuilderChainLink *chain = src->startLink;
	while(chain != NULL) {
		strbAppendMemory(dst, chain->start, chain->last - chain->start);
		chain = chain->next;
	}
	return 1;
}

int32_t strbCompact(stringBuilder *strb) {
	return 0;
}

int32_t strbTrim(stringBuilder *strb/*, int32_t doInfixTrim*/) {
	int32_t reclaimedSpace = 0;
	stringBuilderChainLink **chain = &strb->startLink, *toFree;

	if(strb->startLink == strb->endLink) return 1;

	while(*chain != NULL) {
		if((*chain)->last == (*chain)->start) {
			reclaimedSpace += (*chain)->end - (*chain)->start;

			toFree = *chain;
			toFree->next->prev = toFree->prev;
			toFree->prev->next = toFree->next;

			//TODO free chain and datablock
			return 0;
		}

		chain = &(*chain)->next;
	}

	strb->capacity -= reclaimedSpace;

	return 1;
}

int32_t strbInsertMemory(stringBuilder *strb, int32_t dstPos, char *src, int32_t size) {
	return 0;
}

int32_t strbFragmentingInsertMemory(stringBuilder *strb, int32_t dstPos, char *src, int32_t size) {
	return 0;
}

int32_t strbFormat(stringBuilder *strb, const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	int32_t count = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	char *toAppend = (char*)calloc(count + 1, sizeof(char));

	va_start(args, fmt);
	vsnprintf(toAppend, count + 1, fmt, args);
	va_end(args);

	int32_t success = strbAppendCString(strb, toAppend);

	free(toAppend);
	return success;
}


//Copy paste from ngx_string.c (v1.4.1). Modified to more efficiently use strb_t
int32_t strbEscapeUri(stringBuilder *strb, char *src, int32_t size, int32_t type) {
	int32_t             n;
	uint32_t       *escape;
	static char     hex[] = "0123456789abcdef";

	static uint32_t   uri[] = {
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0x80000029, /* 1000 0000 0000 0000  0000 0000 0010 1001 */
		0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
		0x80000000, /* 1000 0000 0000 0000  0000 0000 0000 0000 */
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0xffffffff  /* 1111 1111 1111 1111  1111 1111 1111 1111 */
	};

	static uint32_t   args[] = {
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0x88000869, /* 1000 1000 0000 0000  0000 1000 0110 1001 */
		0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
		0x80000000, /* 1000 0000 0000 0000  0000 0000 0000 0000 */
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0xffffffff  /* 1111 1111 1111 1111  1111 1111 1111 1111 */
	};

	static uint32_t   uri_component[] = {
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0xfc009fff, /* 1111 1100 0000 0000  1001 1111 1111 1111 */
		0x78000001, /* 0111 1000 0000 0000  0000 0000 0000 0001 */
		0xb8000001, /* 1011 1000 0000 0000  0000 0000 0000 0001 */
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0xffffffff  /* 1111 1111 1111 1111  1111 1111 1111 1111 */
	};

	static uint32_t   html[] = {
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0x000000ad, /* 0000 0000 0000 0000  0000 0000 1010 1101 */
		0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
		0x80000000, /* 1000 0000 0000 0000  0000 0000 0000 0000 */
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0xffffffff  /* 1111 1111 1111 1111  1111 1111 1111 1111 */
	};

	static uint32_t   refresh[] = {
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0x00000085, /* 0000 0000 0000 0000  0000 0000 1000 0101 */
		0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
		0x80000000, /* 1000 0000 0000 0000  0000 0000 0000 0000 */
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0xffffffff  /* 1111 1111 1111 1111  1111 1111 1111 1111 */
	};

	static uint32_t   memcached[] = {
		0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
		0x00000021, /* 0000 0000 0000 0000  0000 0000 0010 0001 */
		0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
		0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
		0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
		0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
		0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
		0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	};

	static uint32_t  *map[] = { //mail_auth is the same as memcached
		uri, args, uri_component, html, refresh, memcached, memcached
	};

	if(strbCurFree(strb) < size * 3 && !strbEnsureContinuousCapacity(strb, size * 3)) return 0;

	n = 0;
	escape = map[type];
	while(size) {
		if(escape[*src >> 5] & (1 << (*src & 0x1f))) {
			*(strbCurLast(strb)++) = '%';
			*(strbCurLast(strb)++) = hex[*src >> 4];
			*(strbCurLast(strb)++) = hex[*src & 0xf];
			src++; n += 3;

		} else {
			*(strbCurLast(strb)++) = *src++;
			n++;
		}
		size--;
	}
	strb->size += n;

	return 1;
}

//Copy paste from ngx_string.c (v1.4.1). Modified to more efficiently use strb_t
int32_t strbEscapeHtml(stringBuilder *strb, char *src, int32_t size) {
	char ch, *dst;
	int32_t  len;
	size_t i;

	len = 0;
	i = size;
	while(i--) {
		switch(*src++) {
		case '<': len += sizeof("&lt;") - 1; break;
		case '>': len += sizeof("&gt;") - 1; break;
		case '&': len += sizeof("&amp;") - 1; break;
		case '"': len += sizeof("&quot;") - 1; break;
		default: len++; break;
		}
	}
	src -= size;

	if(!strbEnsureContinuousCapacity(strb, len)) return 0;

	dst = strbCurLast(strb);
	while(size) {
		ch = *src++;

		switch(ch) {
		case '<': *dst++ = '&'; *dst++ = 'l'; *dst++ = 't'; *dst++ = ';'; break;
		case '>': *dst++ = '&'; *dst++ = 'g'; *dst++ = 't'; *dst++ = ';'; break;
		case '&': *dst++ = '&'; *dst++ = 'a'; *dst++ = 'm'; *dst++ = 'p'; *dst++ = ';'; break;
		case '"': *dst++ = '&'; *dst++ = 'q'; *dst++ = 'u'; *dst++ = 'o'; *dst++ = 't'; *dst++ = ';'; break;
		default: *dst++ = ch; break;
		}

		size--;
	}
	strbCurLast(strb) += len;
	strb->size += len;

	return 1;
}


//This does not use a pool, it uses malloc instead
int32_t strbToCString(stringBuilder *strb, char *dst) {
	strbLog("strb: strbToCString: strb(size=%d, capacity=%d, start=%d, last=%d, end=%d)", strb->size, strb->capacity, strb->startLink, strb->lastLink, strb->endLink);
	stringBuilderChainLink *chain;
	char *last;

	if(dst == NULL) return 0;

	last = dst;
	chain = strb->startLink;

	for(;;) {
		int32_t length = chain->last - chain->start;
		last = (char*)memcpy(last, chain->start, length) + length;
		if(chain == strb->lastLink) break;
		chain = chain->next;
	}
	*last = '\0';

	strbLog("strb: /strbToCString: strb(size=%d, capacity=%d, start=%d, last=%d, end=%d)", strb->size, strb->capacity, strb->startLink, strb->lastLink, strb->endLink);
	return 1;
}

int32_t strbTransformStrb(stringBuilder *dst, stringBuilder *src, strbTransform_fptr strbTransformMethod, ...) {
	int32_t returnCode = 1;
	va_list args;

	char *srcData = (char*)malloc(src->size + 1);
	if(!strbToCString(src, srcData)) return 0;

	va_start(args, strbTransformMethod);
	if(!strbTransformMethod(dst, srcData, src->size, args)) returnCode = 0;
	va_end(args);
	free(srcData);

	return returnCode;
}

int32_t strbTransform(stringBuilder *strb, strbTransform_fptr strbTransformMethod, ...) {
	int32_t size = strb->size;
	char *srcData = (char*)malloc(size + 1);
	if(!strbToCString(strb, srcData)) return 0;
	if(!strbSetSize(strb, 0)) return 0;

	va_list args;
	int32_t returnCode = 1;
	va_start(args, strbTransformMethod);
	if(!strbTransformMethod(strb, srcData, size, args)) returnCode = 0;
	va_end(args);
	free(srcData);

	return returnCode;
}

int32_t strbTransFormat(stringBuilder *strb, char *src, int32_t size, va_list args) {
	return strbFormat(strb, va_arg(args, const char*), src);
}
int32_t strbTransEscapeHtml(stringBuilder *strb, char *src, int32_t size, va_list args) {
	return strbEscapeHtml(strb, src, size);
}
int32_t strbTransEscapeUri(stringBuilder *strb, char *src, int32_t size, va_list args) {
	return strbEscapeUri(strb, src, size, va_arg(args, int32_t));
}

int32_t strbTransPadLeft(stringBuilder *strb, char *src, int32_t size, va_list args) {
	char padChar = va_arg(args, int);
	int32_t toPad = va_arg(args, int32_t);
	if(!strbAppendRepeat(strb, padChar, max(0, toPad - size))) return 0;
	return strbAppendMemory(strb, src, size);
}

int32_t strbTransAsLossyAscii(stringBuilder *strb, char *src, int32_t size, va_list args) {
	if(!strbEnsureContinuousCapacity(strb, size)) return 0;

	char replacementChar = va_arg(args, int32_t);
	char *last = strb->lastLink->last;
	while(size) {
		if(*src & 0x80) {
			size--;
			*last++ = replacementChar;
			strb->size++;

			while(*++src & 0x80) size--;
		} else {
			//Ascii Character
			*last++ = *src++;
			strb->size++;
		}
		size--;
	}
	return 1;
}

void strbDispose(stringBuilder *strb) {
	strb->alloc->dispose(strb);
}