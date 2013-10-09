#include "stringBuilder.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

int32_t stringBuilderAppendChainLinksCalloc(stringBuilder *strb, int32_t count, int32_t size) {
	stringBuilderChainLink *prevLink, *link, *firstLink;

	prevLink = link = firstLink = NULL;
	for(int32_t i = 0; i < count; i++) {
		link = (stringBuilderChainLink*)calloc(1, sizeof(stringBuilderChainLink));
		if(link == NULL) return 0;

		link->start = link->last = (char*)calloc(size, sizeof(char));
		if(link->start == NULL) return 0;

		link->end = link->start + size;

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
		nextLink = link->next;
		free(link->start);
		free(link);
	} while((link = nextLink) != NULL);
}


int32_t strbEnsureCapacity(stringBuilder *strb, int32_t capacity) {
	int32_t neededCapacity = capacity - strb->capacity;
	if(neededCapacity < 1) return 1;

	int32_t count = neededCapacity / strb->alloc->newBufferSize + ((neededCapacity % strb->alloc->newBufferSize) != 0);

	if(!strb->alloc->append(strb, count, strb->alloc->newBufferSize)) return 0;

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

	return 1;
}


int32_t strbEnsureContinuousCapacity(stringBuilder *strb, int32_t capacity) {
	if(capacity < 1 || strbCurFree(strb) >= capacity) return 1;

	int32_t appendedCapacity = min(capacity, strb->alloc->newBufferSize);
	if(!strb->alloc->append(strb, 1, appendedCapacity)) return 0;

	strbUseNextChain(strb);

	return 1;
}

int32_t strbSetSize(stringBuilder *strb, int32_t size) {
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

			if(strbCurFree(strb) == 0) strbUseNextChain(strb);
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

				if(strbCurSize(strb) == 0 && sizeDiff != 0) {
					strb->lastLink = strb->lastLink->prev;
					strb->capacity += strbCurFree(strb);
				}
			}
		}
		strb->size = size;
	}
	return 1;
}

int32_t strbAppendMemory(stringBuilder *strb, char *src, int32_t size) {
	if(size < 0) return 0;
	if(!strbEnsureFreeCapacity(strb, size)) return 0;

	while(size != 0) {
		int32_t copyLength = min(strbCurFree(strb), size);

		strbCurLast(strb) = (char*)memcpy(strbCurLast(strb), src, copyLength) + copyLength;
		strb->size += copyLength;
		size -= copyLength;
		src += copyLength;

		if(strbCurFree(strb) == 0) strbUseNextChain(strb);
	}
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

		if(strbCurFree(strb) == 0) strbUseNextChain(strb);
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
	stringBuilderChainLink **chain = &strb->startLink, *toFree, *prevChain = NULL;

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

		prevChain = *chain;
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

//Copy paste from ngx_string.c (v1.4.1). Modified to more efficiently use strb_t
//static int32_t strbFormatNum(stringBuilder *strb, uint64_t ui64, char zero, uint32_t hexadecimal, uint32_t width) {
//	char         *p, temp[20 + 1]; //Max int64 dec length + null
//	int32_t          len;
//	uint32_t        ui32;
//	static char   hex[] = "0123456789abcdef";
//	static char   HEX[] = "0123456789ABCDEF";
//
//	p = temp + 20; //Max int64 dec length
//
//	if(hexadecimal == 0) {
//		if(ui64 <= (uint64_t)11) { //Max int32 dec length
//			ui32 = (uint32_t)ui64;
//			do {
//				*--p = (char)(ui32 % 10 + '0');
//			} while(ui32 /= 10);
//
//		} else {
//			do {
//				*--p = (char)(ui64 % 10 + '0');
//			} while(ui64 /= 10);
//		}
//
//	} else if(hexadecimal == 1) {
//		do {
//			*--p = hex[(uint32_t)(ui64 & 0xf)];
//		} while(ui64 >>= 4);
//
//	} else {
//		do {
//			*--p = HEX[(uint32_t)(ui64 & 0xf)];
//		} while(ui64 >>= 4);
//	}
//
//	len = (int32_t)width - ((temp + 20) - p); //Max int64 dec length
//	if(len > 0) strbAppendRepeat(strb, zero, len);
//
//	len = (temp + 20) - p; //Max int64 dec length
//	if(!strbAppendMemory(strb, p, len)) return 0;
//
//	return 1;
//}

////Copy paste from ngx_string.c (v1.4.1). Modified to more efficiently use strb_t
//int32_t strbVFormat(stringBuilder *strb, const char *fmt, va_list args) {
//	char                *p, zero;
//	int32_t                    d;
//	double                 f;
//	size_t                 slen;
//	int64_t                i64;
//	uint64_t               ui64, frac;
//	uint32_t               width, sign, hex, max_width, frac_width, scale, n;
//	ngx_msec_t             ms;
//	ngx_str_t             *v;
//	ngx_chain_t           *chain;
//	ngx_variable_value_t  *vv;
//
//	while(*fmt) {
//		if(!strbEnsureFreeCapacity(strb, 1)) return 0;
//		if(strbCurFree(strb) == 0) strbUseNextChain(strb);
//
//		if(*fmt == '%') {
//			zero = (char)((*++fmt == '0') ? '0' : ' ');
//			slen = (size_t)-1;
//			frac_width = 0;
//			max_width = 0;
//			width = 0;
//			sign = 1;
//			hex = 0;
//			ui64 = 0;
//			i64 = 0;
//
//			while(*fmt >= '0' && *fmt <= '9') width = width * 10 + *fmt++ - '0';
//
//			for(;;) {
//				switch(*fmt) {
//				case 'u': sign = 0;          fmt++; continue;
//				case 'm': max_width = 1;     fmt++; continue;
//				case 'X': hex = 2; sign = 0; fmt++; continue;
//				case 'x': hex = 1; sign = 0; fmt++; continue;
//				case '*': slen = va_arg(args, size_t); fmt++; continue;
//				case '.':
//					fmt++;
//					while(*fmt >= '0' && *fmt <= '9') frac_width = frac_width * 10 + *fmt++ - '0';
//					break;
//
//				default: break;
//				}
//
//				break;
//			}
//
//			switch(*fmt) {
//			case 'V':
//				v = va_arg(args, ngx_str_t*);
//				strbAppendNgxString(strb, v);
//				fmt++;
//				continue;
//
//			case 'v':
//				vv = va_arg(args, ngx_variable_value_t*);
//				if(!strbAppendMemory(strb, vv->data, vv->len)) return 0;
//				fmt++;
//				continue;
//
//			case 'C':
//				chain = va_arg(args, ngx_chain_t*);
//				while(chain != NULL) {
//					if(chain->next == NULL) return 0;
//					strbAppendMemory(strb, chain->buf->start, chain->buf->last - chain->buf->start);
//					chain = chain->next;
//				}
//				continue;
//
//			case 's':
//				p = va_arg(args, u_char *);
//				if(slen == (size_t)-1 && width != 0) {
//					slen = strlen((char*)p);
//					width -= slen;
//				}
//
//				if(slen == (size_t)-1) {
//					strbAppendCString(strb, p);
//				} else {
//					if(!strbAppendRepeat(strb, zero, width)) return 0;
//					if(!strbAppendMemory(strb, p, slen)) return 0;
//				}
//				fmt++;
//				continue;
//
//			case 'O': i64 = (int64_t)va_arg(args, off_t);     sign = 1; break;
//			case 'P': i64 = (int64_t)va_arg(args, ngx_pid_t); sign = 1; break;
//			case 'T': i64 = (int64_t)va_arg(args, time_t);    sign = 1; break;
//			case 'M':
//				ms = (ngx_msec_t)va_arg(args, ngx_msec_t);
//				if((ngx_msec_int_t)ms == -1) {
//					sign = 1;
//					i64 = -1;
//				} else {
//					sign = 0;
//					ui64 = (uint64_t)ms;
//				}
//				break;
//
//			case 'z':
//				if(sign) {
//					i64 = (int64_t)va_arg(args, ssize_t);
//				} else {
//					ui64 = (uint64_t)va_arg(args, size_t);
//				}
//				break;
//
//			case 'i':
//				if(sign) {
//					i64 = (int64_t)va_arg(args, ngx_int_t);
//				} else {
//					ui64 = (uint64_t)va_arg(args, ngx_uint_t);
//				}
//				if(max_width) width = 11; //int32 dec length
//				break;
//
//			case 'd':
//				if(sign) {
//					i64 = (int64_t)va_arg(args, int32_t);
//				} else {
//					ui64 = (uint64_t)va_arg(args, u_int);
//				}
//				break;
//
//			case 'l':
//				if(sign) {
//					i64 = (int64_t)va_arg(args, long);
//				} else {
//					ui64 = (uint64_t)va_arg(args, u_long);
//				}
//				break;
//
//			case 'D':
//				if(sign) {
//					i64 = (int64_t)va_arg(args, int32_t);
//				} else {
//					ui64 = (uint64_t)va_arg(args, uint32_t);
//				}
//				break;
//
//			case 'L':
//				if(sign) {
//					i64 = va_arg(args, int64_t);
//				} else {
//					ui64 = va_arg(args, uint64_t);
//				}
//				break;
//
//			case 'A':
//				if(sign) {
//					i64 = (int64_t)va_arg(args, ngx_atomic_int_t);
//				} else {
//					ui64 = (uint64_t)va_arg(args, ngx_atomic_uint_t);
//				}
//				if(max_width) width = NGX_ATOMIC_T_LEN;
//				break;
//
//			case 'f':
//				f = va_arg(args, double);
//
//				if(f < 0) {
//					*(strbCurLast(strb)++) = '-'; strb->size++;
//					f = -f;
//				}
//
//				ui64 = (int64_t)f;
//				frac = 0;
//
//				if(frac_width) {
//					scale = 1;
//					for(n = frac_width; n; n--) scale *= 10;
//
//					frac = (uint64_t)((f - (double)ui64) * scale + 0.5);
//
//					if(frac == scale) {
//						ui64++;
//						frac = 0;
//					}
//				}
//
//				if(!strbFormatNum(strb, ui64, zero, 0, width)) return 0;
//
//				if(frac_width) {
//					*(strbCurLast(strb)++) = '.'; strb->size++;
//					if(!strbFormatNum(strb, ui64, zero, 0, width)) return 0;
//				}
//				fmt++;
//				continue;
//
//#if !(NGX_WIN32)
//			case 'r':
//				i64 = (int64_t)va_arg(args, rlim_t);
//				sign = 1;
//				break;
//#endif
//
//			case 'p':
//				ui64 = (uintptr_t)va_arg(args, void *);
//				hex = 2;
//				sign = 0;
//				zero = '0';
//				width = NGX_PTR_SIZE * 2;
//				break;
//
//			case 'c':
//				d = va_arg(args, int32_t);
//				*(strbCurLast(strb)++) = (u_char)(d & 0xff); strb->size++;
//				fmt++;
//				continue;
//
//			case 'Z':
//				*(strbCurLast(strb)++) = '\0'; strb->size++;
//				fmt++;
//				continue;
//
//			case 'N':
//#if(NGX_WIN32)
//				*(strbCurLast(strb)++) = CR; strb->size++;
//				strbEnsureFreeCapacity(strb, 1);
//#endif
//				*(strbCurLast(strb)++) = LF; strb->size++;
//				fmt++;
//				continue;
//
//			case '%':
//				*(strbCurLast(strb)++) = '%'; strb->size++;
//				fmt++;
//				continue;
//
//			default:
//				*(strbCurLast(strb)++) = *fmt++; strb->size++;
//				continue;
//			}
//
//			if(sign) {
//				if(i64 < 0) {
//					*(strbCurLast(strb)++) = '-'; strb->size++;
//					ui64 = (uint64_t)-i64;
//
//				} else {
//					ui64 = (uint64_t)i64;
//				}
//			}
//			if(!strbFormatNum(strb, ui64, zero, hex, width)) return 0;
//			fmt++;
//
//		} else {
//			*(strbCurLast(strb)++) = *fmt++; strb->size++;
//		}
//	}
//
//	return 1;
//}*/


//Copy paste from ngx_string.c (v1.4.1). Modified to more efficiently use strb_t
//int32_t strbNgxFormat(stringBuilder *strb, const char *fmt, ...) {
//	int32_t rc;
//	va_list args;
//
//	va_start(args, fmt);
//	rc = strbVNgxFormat(strb, fmt, args);
//	va_end(args);
//
//	return rc;
//}

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
	char *srcData = (char*)malloc(strb->size + 1);
	if(!strbToCString(strb, srcData)) return 0;
	if(!strbSetSize(strb, 0)) return 0;

	va_list args;
	int32_t returnCode = 1;
	va_start(args, strbTransformMethod);
	if(!strbTransformMethod(strb, srcData, strb->size, args)) returnCode = 0;
	va_end(args);
	free(srcData);

	return returnCode;
}

int32_t strbTransFormat(stringBuilder *strb, char *src, int32_t size, va_list args) {
	return strbFormat(strb, va_arg(args, const char*), src);
}
//int32_t strbTransNgxFormat(stringBuilder *strb, char *src, int32_t size, va_list args) {
//	return strbNgxFormat(strb, va_arg(args, const char*), src);
//}
int32_t strbTransEscapeHtml(stringBuilder *strb, char *src, int32_t size, va_list args) {
	return strbEscapeHtml(strb, src, size);
}
int32_t strbTransEscapeUri(stringBuilder *strb, char *src, int32_t size, va_list args) {
	return strbEscapeUri(strb, src, size, va_arg(args, int32_t));
}

void strbDispose(stringBuilder *strb) {
	strb->alloc->dispose(strb);
}