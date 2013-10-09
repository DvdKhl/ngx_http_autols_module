#ifndef _NGX_STRING_BUILDER_INCLUDED_
#define _NGX_HTTP_AUTOLS_MODULE_INCLUDED_

#include "stringBuilder.h"
#include <ngx_core.h>


#define strbAppendNgxString(strb, str) strbAppendMemory(strb, (char*)(str)->data, (str)->len)

int32_t strbVNgxFormat(stringBuilder *strb, const char *fmt, va_list args);
int32_t strbNgxFormat(stringBuilder *strb, const char *fmt, ...);

#endif