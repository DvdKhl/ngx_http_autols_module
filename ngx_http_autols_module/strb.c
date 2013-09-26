#include "strb.h"

int strbInit(strb_t *strb, ngx_pool_t *pool, int32_t newBufferSize, int32_t minCapacity) {
    ngx_bufs_t bufs;

    if(minCapacity < 0) return 0;
    if(minCapacity == 0) minCapacity = 1;

    bufs.num = minCapacity / newBufferSize + (minCapacity % newBufferSize != 0);
    bufs.size = newBufferSize;

    strb->pool = pool;
    strb->newBufferSize = newBufferSize;

    strb->capacity = bufs.num * bufs.size;

    strb->startLink = strb->lastLink = ngx_create_chain_of_bufs(pool, &bufs);
    if(strb->startLink == NULL) return 0;

    strb->endLink = strb->startLink + bufs.num;
    strb->lastLink->buf->last_in_chain = 1;

    strb->isInitialized = 1;

    return 1;
}

int strbEnsureCapacity(strb_t *strb, int32_t capacity) {
    int32_t neededCapacity = capacity - strbFree(strb);
    ngx_bufs_t bufs;

    if(neededCapacity < 1) return 1;

    bufs.size = strb->newBufferSize;
    bufs.num = (ngx_int_t)(neededCapacity / strb->newBufferSize + ((neededCapacity % strb->newBufferSize) != 0));

    strb->lastLink->buf->last_in_chain = 0;
    strb->endLink->next = ngx_create_chain_of_bufs(strb->pool, &bufs) + bufs.num;
    if(strb->endLink == NULL) return 0;
    strb->endLink->buf->last_in_chain = 1;

    return 1;
}

int strbEnsureContinuousCapacity(strb_t *strb, int32_t capacity) {
    ngx_chain_t *chain;

    if(capacity < 1) return 1;
    if(strbCurLast(strb) == strbCurEnd(strb)) strbUseNextChain(strb);
    if(strb->lastLink != NULL && strbCurFree(strb) >= capacity) return 1;
    
    chain = ngx_alloc_chain_link(strb->pool);
    if(chain == NULL) return 0;

    chain->buf = ngx_create_temp_buf(strb->pool, capacity);
    if(chain->buf == NULL) return 0;

    if(strb->lastLink == NULL || strbCurStart(strb) == strbCurLast(strb)) {
        chain->next = strb->lastLink;
        strb->lastLink = chain;

    } else {
        ngx_chain_t *restChain = ngx_alloc_chain_link(strb->pool);
        if(restChain == NULL) return 0;

        restChain->buf = (ngx_buf_t*)ngx_calloc_buf(strb->pool);
        if(restChain->buf == NULL) return 0;

        restChain->buf->temporary = 1;
        restChain->buf->pos = strbCurLast(strb);
        restChain->buf->last = strbCurLast(strb);
        restChain->buf->start = strbCurLast(strb);
        restChain->buf->end = restChain->buf->start + strbCurFree(strb);

        strbCurEnd(strb) = strbCurLast(strb);

        restChain->next = strb->lastLink->next;
        chain->next = restChain;
        strb->lastLink->next = chain;
    }

    strb->capacity += capacity;
    strb->lastLink = chain;

    return 1;
}

int strbSetSize(strb_t *strb, int32_t size) {
    if(size < 0) return 0;
    if(!strbEnsureCapacity(strb, size)) return 0;

    if(strb->size < size) {
        size = size - strb->size;
        while(size) {
            int32_t toSet = ngx_min(strbCurFree(strb), size);
            ngx_memzero(strbCurLast(strb), size);
            strbCurLast(strb) += toSet;

            strb->size += toSet;
            size -= toSet;

            if(size != 0) strbUseNextChain(strb);
        }

    } else {
        if(strbCurSize(strb) >= strb->size - size) { //Check if current chain is the one we're looking for
            strbCurLast(strb) -= strb->size - size;

        } else { //Seek chain from startlink
            int32_t toSeek = size;
            ngx_chain_t *curChain = strb->startLink;

            while(curChain != NULL && toSeek >= curChain->buf->last - curChain->buf->start) {
                toSeek -= curChain->buf->last - curChain->buf->start;
                curChain = curChain->next;
            }
            if(curChain == NULL) return 0;

            strb->lastLink->buf->last_in_chain = 0;
            strb->lastLink = curChain;

            curChain->buf->last_in_chain = 1;
            curChain->buf->last = curChain->buf->start + toSeek;

            curChain = curChain->next;
            while(curChain != NULL) curChain->buf->last = curChain->buf->start;
        }
        strb->size = size;
    }
    return 1;
}

int strbAppendMemory(strb_t *strb, u_char *src, int32_t size) {
    int32_t unusedSpace, copyLength;
    u_char *srcLimit = src + size;

    if(size < 0) return 0;
    if(!strbEnsureFreeCapacity(strb, size)) return 0;

    while(src < srcLimit) {
        unusedSpace = strbCurFree(strb);
        copyLength = ngx_min(unusedSpace, size);

        strbCurLast(strb) = ngx_cpymem(strbCurLast(strb), src, copyLength);
        strb->size += copyLength;
        src += copyLength;

        if(copyLength == 0) strbUseNextChain(strb);
    }
    return 1;
}

int strbAppendSingle(strb_t *strb, u_char value) {
    if(!strbEnsureFreeCapacity(strb, 1)) return 0;
    if(strbCurFree(strb) < 1) strbUseNextChain(strb);
    *(strbCurLast(strb)++) = value;
    strb->size++;
    return 1;
}

int strbAppendPad(strb_t *strb, u_char c, int32_t padLength) {
    if(!strbEnsureFreeCapacity(strb, padLength)) return 0;

    strb->size += padLength;
    while(padLength != 0) {
        int32_t toPad = ngx_min(padLength, strbCurFree(strb));
        ngx_memset(strbCurLast(strb), c, toPad);
        strbCurLast(strb) += toPad;
        padLength -= toPad;

        if(padLength != 0) strbUseNextChain(strb);
    }

    return 1;
}

int strbAppendStrb(strb_t *dst, strb_t *src) {
    ngx_chain_t *chain = src->startLink;
    while(chain != NULL) {
        if(chain->buf == NULL) return 0;
        strbAppendMemory(dst, chain->buf->start, chain->buf->last - chain->buf->start);
        chain = chain->next;
    }
    return 1;
}

int strbCompact(strb_t *strb) {
    //ngx_chain_t *chain = strb->startLink;

    return 0;
}

int strbTrim(strb_t *strb/*, int doInfixTrim*/) {
    int32_t reclaimedSpace = 0;
    ngx_chain_t **chain = &strb->startLink, *toFree, *prevChain = NULL;

    if(strb->startLink == strb->endLink) return 1;

    while(*chain != NULL) {
        if((*chain)->buf->last == (*chain)->buf->start) {
            reclaimedSpace += (*chain)->buf->end - (*chain)->buf->start;

            toFree = *chain;
            *chain = (*chain)->next;

            toFree->buf->last_in_chain = 0;
            ngx_free_chain(strb->pool, toFree);
        }

        prevChain = *chain;
        chain = &(*chain)->next;
    }
    prevChain->buf->last_in_chain = 1;

    strb->capacity -= reclaimedSpace;

    return 1;
}

int strbInsertMemory(strb_t *strb, size_t dstPos, u_char *src, int32_t size) {
    return 0;
}

int strbFragmentingInsertMemory(strb_t *strb, int32_t dstPos, u_char *src, int32_t size) {
    return 0;
}

//Copy paste from ngx_string.c (v1.4.1). Modified to more efficiently use strb_t
static int strbFormatNum(strb_t *strb, uint64_t ui64, u_char zero, ngx_uint_t hexadecimal, ngx_uint_t width) {
    u_char         *p, temp[NGX_INT64_LEN + 1];
    size_t          len;
    uint32_t        ui32;
    static u_char   hex[] = "0123456789abcdef";
    static u_char   HEX[] = "0123456789ABCDEF";

    p = temp + NGX_INT64_LEN;

    if(hexadecimal == 0) {
        if(ui64 <= (uint64_t) NGX_MAX_UINT32_VALUE) {
            ui32 = (uint32_t) ui64;
            do { *--p = (u_char) (ui32 % 10 + '0'); } while (ui32 /= 10);

        } else {
            do { *--p = (u_char) (ui64 % 10 + '0'); } while (ui64 /= 10);
        }

    } else if(hexadecimal == 1) {
        do { *--p = hex[(uint32_t) (ui64 & 0xf)]; } while (ui64 >>= 4);

    } else {
        do { *--p = HEX[(uint32_t) (ui64 & 0xf)]; } while (ui64 >>= 4);
    }

    len = (temp + NGX_INT64_LEN) - p;
    while(len++ < width) if(!strbAppendSingle(strb, zero)) return 0;

    len = (temp + NGX_INT64_LEN) - p;
    if(!strbAppendMemory(strb, p, len)) return 0;

    return 1;
}

//Copy paste from ngx_string.c (v1.4.1). Modified to more efficiently use strb_t
int strbVFormat(strb_t *strb, const char *fmt, va_list args) {
    u_char                *p, zero;
    int                    d;
    double                 f;
    size_t                 slen;
    int64_t                i64;
    uint64_t               ui64, frac;
    ngx_msec_t             ms;
    ngx_uint_t             width, sign, hex, max_width, frac_width, scale, n;
    ngx_str_t             *v;
    ngx_chain_t           *chain;
    ngx_variable_value_t  *vv;

    while(*fmt) {
        if(!strbEnsureFreeCapacity(strb, 1)) return 0;

        if(*fmt == '%') {
            zero = (u_char)((*++fmt == '0') ? '0' : ' ');
            slen = (size_t)-1;
            frac_width = 0;
            max_width = 0;
            width = 0;
            sign = 1;
            hex = 0;
            ui64 = 0;
            i64 = 0;

            while (*fmt >= '0' && *fmt <= '9') width = width * 10 + *fmt++ - '0';

            for(;;) {
                switch (*fmt) {
                case 'u': sign = 0;          fmt++; continue;
                case 'm': max_width = 1;     fmt++; continue;
                case 'X': hex = 2; sign = 0; fmt++; continue;
                case 'x': hex = 1; sign = 0; fmt++; continue;
                case '*': slen = va_arg(args, size_t); fmt++; continue;
                case '.':
                    fmt++;
                    while(*fmt >= '0' && *fmt <= '9') frac_width = frac_width * 10 + *fmt++ - '0';
                    break;

                default: break;
                }

                break;
            }

            switch (*fmt) {
            case 'V':
                v = va_arg(args, ngx_str_t*);
                strbAppendNgxString(strb, v);
                fmt++;
                continue;

            case 'v':
                vv = va_arg(args, ngx_variable_value_t*);
                if(!strbAppendMemory(strb, vv->data, vv->len)) return 0;
                fmt++;
                continue;

            case 'C':
                chain = va_arg(args, ngx_chain_t*);
                while(chain != NULL) {
                    if(chain->next == NULL) return 0;
                    strbAppendMemory(strb, chain->buf->start, chain->buf->last - chain->buf->start);
                    chain = chain->next;
                }
                continue;

            case 's':
                p = va_arg(args, u_char *);

                if(slen == (size_t)-1) {
                    strbAppendCString(strb, p); 
                } else {
                    if(!strbAppendMemory(strb, p, slen)) return 0;
                }
                fmt++;
                continue;

            case 'O': i64 = (int64_t) va_arg(args, off_t);     sign = 1; break;
            case 'P': i64 = (int64_t) va_arg(args, ngx_pid_t); sign = 1; break;
            case 'T': i64 = (int64_t) va_arg(args, time_t);    sign = 1; break;
            case 'M':
                ms = (ngx_msec_t) va_arg(args, ngx_msec_t);
                if((ngx_msec_int_t) ms == -1) {
                    sign = 1;
                    i64 = -1;
                } else {
                    sign = 0;
                    ui64 = (uint64_t) ms;
                }
                break;

            case 'z':
                if(sign) {
                    i64 = (int64_t) va_arg(args, ssize_t);
                } else {
                    ui64 = (uint64_t) va_arg(args, size_t);
                }
                break;

            case 'i':
                if(sign) {
                    i64 = (int64_t) va_arg(args, ngx_int_t);
                } else {
                    ui64 = (uint64_t) va_arg(args, ngx_uint_t);
                }
                if(max_width) width = NGX_INT_T_LEN;
                break;

            case 'd':
                if(sign) {
                    i64 = (int64_t) va_arg(args, int);
                } else {
                    ui64 = (uint64_t) va_arg(args, u_int);
                }
                break;

            case 'l':
                if(sign) {
                    i64 = (int64_t) va_arg(args, long);
                } else {
                    ui64 = (uint64_t) va_arg(args, u_long);
                }
                break;

            case 'D':
                if(sign) {
                    i64 = (int64_t) va_arg(args, int32_t);
                } else {
                    ui64 = (uint64_t) va_arg(args, uint32_t);
                }
                break;

            case 'L':
                if(sign) {
                    i64 = va_arg(args, int64_t);
                } else {
                    ui64 = va_arg(args, uint64_t);
                }
                break;

            case 'A':
                if(sign) {
                    i64 = (int64_t) va_arg(args, ngx_atomic_int_t);
                } else {
                    ui64 = (uint64_t) va_arg(args, ngx_atomic_uint_t);
                }
                if(max_width) width = NGX_ATOMIC_T_LEN;
                break;

            case 'f':
                f = va_arg(args, double);

                if(f < 0) {
                    if(!strbAppendSingle(strb, '-')) return 0;
                    f = -f;
                }

                ui64 = (int64_t) f;
                frac = 0;

                if(frac_width) {
                    scale = 1;
                    for(n = frac_width; n; n--) scale *= 10;

                    frac = (uint64_t) ((f - (double) ui64) * scale + 0.5);

                    if(frac == scale) {
                        ui64++;
                        frac = 0;
                    }
                }

                if(!strbFormatNum(strb, ui64, zero, 0, width)) return 0;

                if(frac_width) {
                    if(!strbAppendSingle(strb, '.')) return 0;
                    if(!strbFormatNum(strb, ui64, zero, 0, width)) return 0;
                }
                fmt++;
                continue;

#if !(NGX_WIN32)
            case 'r':
                i64 = (int64_t) va_arg(args, rlim_t);
                sign = 1;
                break;
#endif

            case 'p':
                ui64 = (uintptr_t) va_arg(args, void *);
                hex = 2;
                sign = 0;
                zero = '0';
                width = NGX_PTR_SIZE * 2;
                break;

            case 'c':
                d = va_arg(args, int);
                if(!strbAppendSingle(strb, (u_char)(d & 0xff))) return 0;
                fmt++;
                continue;

            case 'Z':
                if(!strbAppendSingle(strb, '\0')) return 0;
                fmt++;
                continue;

            case 'N':
#if(NGX_WIN32)
                if(!strbAppendSingle(strb, CR)) return 0;
#endif
                if(!strbAppendSingle(strb, LF)) return 0;
                fmt++;
                continue;

            case '%':
                if(!strbAppendSingle(strb, '%')) return 0;
                fmt++;
                continue;

            default:
                if(!strbAppendSingle(strb, *fmt++)) return 0;
                continue;
            }

            if(sign) {
                if(i64 < 0) {
                    if(!strbAppendSingle(strb, '-')) return 0;
                    ui64 = (uint64_t) -i64;

                } else {
                    ui64 = (uint64_t) i64;
                }
            }
            if(!strbFormatNum(strb, ui64, zero, hex, width)) return 0;
            fmt++;

        } else {
            if(!strbAppendSingle(strb, *fmt++)) return 0;
        }
    }

    return 1;
}

//Copy paste from ngx_string.c (v1.4.1). Modified to more efficiently use strb_t
int strbFormat(strb_t *strb, const char *fmt, ...) {
    int rc;
    va_list args;

    va_start(args, fmt);
    rc = strbVFormat(strb, fmt, args);
    va_end(args);

    return rc;
}

//Copy paste from ngx_string.c (v1.4.1). Modified to more efficiently use strb_t
int strbEscapeUri(strb_t *strb, u_char *src, int32_t size, uint32_t type) {
    ngx_uint_t      n;
    uint32_t       *escape;
    static u_char   hex[] = "0123456789abcdef";

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
        if (escape[*src >> 5] & (1 << (*src & 0x1f))) {
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
int strbEscapeHtml(strb_t *strb, u_char *src, int32_t size) {
    u_char      ch, *dst;
    ngx_uint_t  len;
    size_t i;

    len = 0;
    i = size;
    while(i--) {
        switch (*src++) {
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

        switch (ch) {
        case '<': *dst++ = '&'; *dst++ = 'l'; *dst++ = 't'; *dst++ = ';'; break;
        case '>': *dst++ = '&'; *dst++ = 'g'; *dst++ = 't'; *dst++ = ';'; break;
        case '&': *dst++ = '&'; *dst++ = 'a'; *dst++ = 'm'; *dst++ = 'p'; *dst++ = ';'; break;
        case '"': *dst++ = '&'; *dst++ = 'q'; *dst++ = 'u'; *dst++ = 'o'; *dst++ = 't'; *dst++ = ';'; break;
        default: *dst++ = ch; break;
        }

        size--;
    }
    strbCurLast(strb) += len;

    return 1;
}


//This does not use a pool, it uses malloc instead
int strbToCString(strb_t *strb, u_char **data) {
    ngx_chain_t *chain;
    u_char *last;

    *data = (u_char*)malloc(strb->size + 1);
    if(*data == NULL) return 0;

    last = *data;
    chain = strb->startLink;

    for(;;) {
        last = ngx_cpymem(last, chain->buf->start, chain->buf->last - chain->buf->start);
        if(chain == strb->lastLink) break;
        chain = chain->next;
    }
    last = 0;

    return 1;
}

int strbTransformStrb(strb_t *dst, strb_t *src, strbTransform_fptr strbTransformMethod, ...) {
    int returnCode = 1;
    u_char *srcData;
    va_list args;

    if(!strbToCString(src, &srcData)) return 0;

    va_start(args, strbTransformMethod);
    if(!strbTransformMethod(dst, srcData, src->size, args)) returnCode = 0;
    va_end(args);
    free(srcData);

    return returnCode;
}

int strbTransFormat(strb_t *strb, u_char *src, int32_t size, va_list args) {
    return strbFormat(strb, va_arg(args, const char*), src);
}
int strbTransEscapeHtml(strb_t *strb, u_char *src, int32_t size, va_list args) {
    return strbEscapeHtml(strb, src, size);
}
int strbTransEscapeUri(strb_t *strb, u_char *src, int32_t size, va_list args) {
    return strbEscapeUri(strb, src, size, va_arg(args, uint32_t));
}
