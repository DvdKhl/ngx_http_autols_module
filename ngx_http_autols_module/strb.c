#include "strb.h"

int strbInit(strb_t *strb, ngx_pool_t *pool, size_t newBufferSize, size_t capacity) {
    ngx_bufs_t bufs;
    
    if(capacity == 0) capacity = 1;

    bufs.num = capacity / newBufferSize + (capacity % newBufferSize != 0);
    bufs.size = newBufferSize;

    strb->pool = pool;
    strb->newBufferSize = newBufferSize;

    strb->size = strb->fragmentedCapacity = 0;
    strb->capacity = bufs.num * bufs.size;

    strb->firstLink = strb->currentLink = ngx_create_chain_of_bufs(pool, &bufs);
    if(strb->firstLink == NULL) return 0;

    strb->lastLink = strb->firstLink + bufs.num;
    strb->currentLink->buf->last_in_chain = 1;

    return 1;
}

int strbEnsureCapacity(strb_t *strb, size_t capacityEnsurance) {
    off_t neededCapacity = capacityEnsurance - strbFree(strb);
    ngx_bufs_t bufs;

    if(neededCapacity < 1) return 1;
    bufs.num = (ngx_int_t)(neededCapacity / strb->newBufferSize + (neededCapacity % strb->newBufferSize != 0));
    bufs.size = strb->newBufferSize;

    strb->currentLink->buf->last_in_chain = 0;
    strb->lastLink->next = ngx_create_chain_of_bufs(strb->pool, &bufs) + bufs.num;
    if(strb->lastLink == NULL) return 0;
    strb->lastLink->buf->last_in_chain = 1;

    return 1;
}

int strbAppendMemory(strb_t *strb, u_char *src, size_t size) {
    size_t unusedSpace, copyLength;
    u_char *srcLimit = src + size;

    if(!strbEnsureCapacity(strb, size)) return 0;

    while(src < srcLimit) {
        unusedSpace = strbCurFree(strb);
        copyLength = ngx_min(unusedSpace, size);

        strbCurLast(strb) = ngx_cpymem(strbCurLast(strb), src, copyLength);
        strb->size += copyLength;
        src += copyLength;

        if(copyLength == 0) strb->currentLink++;
    }
    return 1;
}

int strbSetSize(strb_t *strb, size_t size) {
    ngx_int_t sizeDiff = size - strb->size;
    if(sizeDiff > 0) if(!strbEnsureCapacity(strb, sizeDiff)) return 0;

    if(sizeDiff < 0) {
        sizeDiff *= -1;
        while(sizeDiff) {
            size_t toClear = ngx_min(strbCurSize(strb), sizeDiff);
            strbCurBuf(strb)->last -= toClear;

            sizeDiff -= toClear;
            strb->size -= toClear;

            if(sizeDiff != 0) {
                strb->currentLink--;
                strb->fragmentedCapacity -= strbCurFree(strb);
            }
        }

    } else {
        while(sizeDiff) {
            size_t toSet = ngx_min(strbCurFree(strb), sizeDiff);
            ngx_memzero(strbCurLast(strb), sizeDiff);
            strbCurBuf(strb)->last += toSet;

            strb->size += toSet;
            sizeDiff -= toSet;

            if(sizeDiff != 0) strb->currentLink++;
        }
    }
    return 1;
}

int strbAppendSingle(strb_t *strb, u_char value) {
    if(!strbEnsureCapacity(strb, 1)) return 0;
    if(strbCurFree(strb) < 1) strb->currentLink++;
    *(strbCurLast(strb)++) = value;
    strb->size++;
    return 1;
}

int strbCompact(strb_t *strb) {
    //ngx_chain_t *chain = strb->firstLink;

    return 0;
}

int strbTrim(strb_t *strb/*, int doInfixTrim*/) {
    size_t reclaimedSpace = 0, reclaimedFragmentedSpace = 0;
    ngx_chain_t **chain = &strb->firstLink, *toFree, *prevChain = NULL;

    if(strb->firstLink == strb->lastLink) return 1;

    while(*chain != NULL) {
        if((*chain)->buf->last == (*chain)->buf->pos) {
            reclaimedSpace += (*chain)->buf->end - (*chain)->buf->pos;

            toFree = *chain;
            *chain = (*chain)->next;

            toFree->buf->last_in_chain = 0;
            ngx_free_chain(strb->pool, toFree);

        } else {
            reclaimedFragmentedSpace = reclaimedSpace;
        }

        prevChain = *chain;
        chain = &(*chain)->next;
    }
    prevChain->buf->last_in_chain = 1;

    strb->capacity -= reclaimedSpace;
    strb->fragmentedCapacity -= reclaimedFragmentedSpace;

    return 1;
}

int strbInsertMemory(strb_t *strb, size_t dstPos, u_char *src, size_t size) {
    return 0;
}
int strbFragmentingInsertMemory(strb_t *strb, size_t dstPos, u_char *src, size_t size) {
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
    ngx_variable_value_t  *vv;

    while(*fmt) {
        if(!strbEnsureCapacity(strb, 1)) return 0;

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
                    while (*fmt >= '0' && *fmt <= '9') frac_width = frac_width * 10 + *fmt++ - '0';
                    break;

                default: break;
                }

                break;
            }

            switch (*fmt) {
            case 'V':
                v = va_arg(args, ngx_str_t *);
                strbAppendNgxString(strb, v);
                fmt++;
                continue;

            case 'v':
                vv = va_arg(args, ngx_variable_value_t *);
                if(!strbAppendMemory(strb, vv->data, vv->len)) return 0;
                fmt++;
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
