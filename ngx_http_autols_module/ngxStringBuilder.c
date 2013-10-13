#include "ngxStringBuilder.h"



//Copy paste from ngx_string.c (v1.4.1). Modified to more efficiently use strb_t
static int32_t strbNgxFormatNum(stringBuilder *strb, uint64_t ui64, char zero, uint32_t hexadecimal, uint32_t width) {
	char         *p, temp[20 + 1]; //Max int64 dec length + null
	int32_t          len;
	uint32_t        ui32;
	static char   hex[] = "0123456789abcdef";
	static char   HEX[] = "0123456789ABCDEF";

	p = temp + 20; //Max int64 dec length

	if(hexadecimal == 0) {
		if(ui64 <= (uint64_t)11) { //Max int32 dec length
			ui32 = (uint32_t)ui64;
			do {
				*--p = (char)(ui32 % 10 + '0');
			} while(ui32 /= 10);

		} else {
			do {
				*--p = (char)(ui64 % 10 + '0');
			} while(ui64 /= 10);
		}

	} else if(hexadecimal == 1) {
		do {
			*--p = hex[(uint32_t)(ui64 & 0xf)];
		} while(ui64 >>= 4);

	} else {
		do {
			*--p = HEX[(uint32_t)(ui64 & 0xf)];
		} while(ui64 >>= 4);
	}

	len = (int32_t)width - ((temp + 20) - p); //Max int64 dec length
	if(len > 0) strbAppendRepeat(strb, zero, len);

	len = (temp + 20) - p; //Max int64 dec length
	if(!strbAppendMemory(strb, p, len)) return 0;

	return 1;
}

//Copy paste from ngx_string.c (v1.4.1). Modified to more efficiently use strb_t
int32_t strbVNgxFormat(stringBuilder *strb, const char *fmt, va_list args) {
	char                *p, zero;
	int32_t                    d;
	double                 f;
	size_t                 slen;
	int64_t                i64;
	uint64_t               ui64, frac;
	uint32_t               width, sign, hex, max_width, frac_width, scale, n;
	ngx_msec_t             ms;
	ngx_str_t             *v;
	ngx_chain_t           *chain;
	ngx_variable_value_t  *vv;

	while(*fmt) {
		if(!strbEnsureFreeCapacity(strb, 1)) return 0;
		if(strbCurFree(strb) == 0) strbUseNextChain(strb);

		if(*fmt == '%') {
			zero = (char)((*++fmt == '0') ? '0' : ' ');
			slen = (size_t)-1;
			frac_width = 0;
			max_width = 0;
			width = 0;
			sign = 1;
			hex = 0;
			ui64 = 0;
			i64 = 0;

			while(*fmt >= '0' && *fmt <= '9') width = width * 10 + *fmt++ - '0';

			for(;;) {
				switch(*fmt) {
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

			switch(*fmt) {
			case 'V':
				v = va_arg(args, ngx_str_t*);
				strbAppendNgxString(strb, v);
				fmt++;
				continue;

			case 'v':
				vv = va_arg(args, ngx_variable_value_t*);
				if(!strbAppendMemory(strb, (char*)vv->data, vv->len)) return 0;
				fmt++;
				continue;

			case 'C':
				chain = va_arg(args, ngx_chain_t*);
				while(chain != NULL) {
					if(chain->next == NULL) return 0;
					strbAppendMemory(strb, (char*)chain->buf->start, chain->buf->last - chain->buf->start);
					chain = chain->next;
				}
				continue;

			case 's':
				p = va_arg(args, char*);
				if(slen == (size_t)-1 && width != 0) {
					slen = strlen((char*)p);
					width -= slen;
				}

				if(slen == (size_t)-1) {
					strbAppendCString(strb, p);
				} else {
					if(!strbAppendRepeat(strb, zero, width)) return 0;
					if(!strbAppendMemory(strb, p, slen)) return 0;
				}
				fmt++;
				continue;

			case 'O': i64 = (int64_t)va_arg(args, off_t);     sign = 1; break;
			case 'P': i64 = (int64_t)va_arg(args, ngx_pid_t); sign = 1; break;
			case 'T': i64 = (int64_t)va_arg(args, time_t);    sign = 1; break;
			case 'M':
				ms = (ngx_msec_t)va_arg(args, ngx_msec_t);
				if((ngx_msec_int_t)ms == -1) {
					sign = 1;
					i64 = -1;
				} else {
					sign = 0;
					ui64 = (uint64_t)ms;
				}
				break;

			case 'z':
				if(sign) {
					i64 = (int64_t)va_arg(args, ssize_t);
				} else {
					ui64 = (uint64_t)va_arg(args, size_t);
				}
				break;

			case 'i':
				if(sign) {
					i64 = (int64_t)va_arg(args, ngx_int_t);
				} else {
					ui64 = (uint64_t)va_arg(args, ngx_uint_t);
				}
				if(max_width) width = 11; //int32 dec length
				break;

			case 'd':
				if(sign) {
					i64 = (int64_t)va_arg(args, int32_t);
				} else {
					ui64 = (uint64_t)va_arg(args, u_int);
				}
				break;

			case 'l':
				if(sign) {
					i64 = (int64_t)va_arg(args, long);
				} else {
					ui64 = (uint64_t)va_arg(args, u_long);
				}
				break;

			case 'D':
				if(sign) {
					i64 = (int64_t)va_arg(args, int32_t);
				} else {
					ui64 = (uint64_t)va_arg(args, uint32_t);
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
					i64 = (int64_t)va_arg(args, ngx_atomic_int_t);
				} else {
					ui64 = (uint64_t)va_arg(args, ngx_atomic_uint_t);
				}
				if(max_width) width = NGX_ATOMIC_T_LEN;
				break;

			case 'f':
				f = va_arg(args, double);

				if(f < 0) {
					*(strbCurLast(strb)++) = '-'; strb->size++;
					f = -f;
				}

				ui64 = (int64_t)f;
				frac = 0;

				if(frac_width) {
					scale = 1;
					for(n = frac_width; n; n--) scale *= 10;

					frac = (uint64_t)((f - (double)ui64) * scale + 0.5);

					if(frac == scale) {
						ui64++;
						frac = 0;
					}
				}

				if(!strbNgxFormatNum(strb, ui64, zero, 0, width)) return 0;

				if(frac_width) {
					*(strbCurLast(strb)++) = '.'; strb->size++;
					if(!strbNgxFormatNum(strb, ui64, zero, 0, width)) return 0;
				}
				fmt++;
				continue;

#if !(NGX_WIN32)
			case 'r':
				i64 = (int64_t)va_arg(args, rlim_t);
				sign = 1;
				break;
#endif

			case 'p':
				ui64 = (uintptr_t)va_arg(args, void *);
				hex = 2;
				sign = 0;
				zero = '0';
				width = NGX_PTR_SIZE * 2;
				break;

			case 'c':
				d = va_arg(args, int32_t);
				*(strbCurLast(strb)++) = (u_char)(d & 0xff); strb->size++;
				fmt++;
				continue;

			case 'Z':
				*(strbCurLast(strb)++) = '\0'; strb->size++;
				fmt++;
				continue;

			case 'N':
#if(NGX_WIN32)
				*(strbCurLast(strb)++) = CR; strb->size++;
				strbEnsureFreeCapacity(strb, 1);
#endif
				*(strbCurLast(strb)++) = LF; strb->size++;
				fmt++;
				continue;

			case '%':
				*(strbCurLast(strb)++) = '%'; strb->size++;
				fmt++;
				continue;

			default:
				*(strbCurLast(strb)++) = *fmt++; strb->size++;
				continue;
			}

			if(sign) {
				if(i64 < 0) {
					*(strbCurLast(strb)++) = '-'; strb->size++;
					ui64 = (uint64_t)-i64;

				} else {
					ui64 = (uint64_t)i64;
				}
			}
			if(!strbNgxFormatNum(strb, ui64, zero, hex, width)) return 0;
			fmt++;

		} else {
			*(strbCurLast(strb)++) = *fmt++; strb->size++;
		}
	}

	return 1;
}


//Copy paste from ngx_string.c (v1.4.1). Modified to more efficiently use strb_t
int32_t strbNgxFormat(stringBuilder *strb, const char *fmt, ...) {
	int32_t rc;
	va_list args;

	va_start(args, fmt);
	rc = strbVNgxFormat(strb, fmt, args);
	va_end(args);

	return rc;
}


int32_t strbNgxUtf8Length(stringBuilder *strb) {
	int32_t length = 0;
	stringBuilderChainLink *link = strb->startLink;
	stringBuilderChainLink *linkLimit = strb->lastLink->next;
	do {
		length += ngx_utf8_length((u_char*)link->start, link->last - link->start);
	} while((link = link->next) != linkLimit);

	return length;
}
