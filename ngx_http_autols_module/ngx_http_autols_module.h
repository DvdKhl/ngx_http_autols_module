#ifndef _NGX_HTTP_AUTOLS_MODULE_INCLUDED_
#define _NGX_HTTP_AUTOLS_MODULE_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngxStringBuilder.h"

#if NGX_PCRE && !_MSC_VER
#define USE_REGEX 1
#else
#define USE_REGEX 0
#endif


#define STRING_PREALLOCATE  50
#if STRING_PREALLOCATE < 1
#error STRING_PREALLOCATE must at least be higher than 0
#endif

#define CLOSE_DIRECTORY_OK 0
#define CLOSE_DIRECTORY_ERROR 1


#define logHttpDebugMsg0(log, fmt)                                                 ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0, fmt)
#define logHttpDebugMsg1(log, fmt, arg1)                                           ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, fmt, arg1)
#define logHttpDebugMsg2(log, fmt, arg1, arg2)                                     ngx_log_debug2(NGX_LOG_DEBUG_HTTP, log, 0, fmt, arg1, arg2)
#define logHttpDebugMsg3(log, fmt, arg1, arg2, arg3)                               ngx_log_debug3(NGX_LOG_DEBUG_HTTP, log, 0, fmt, arg1, arg2, arg3)
#define logHttpDebugMsg4(log, fmt, arg1, arg2, arg3, arg4)                         ngx_log_debug4(NGX_LOG_DEBUG_HTTP, log, 0, fmt, arg1, arg2, arg3, arg4)
#define logHttpDebugMsg5(log, fmt, arg1, arg2, arg3, arg4, arg5)                   ngx_log_debug5(NGX_LOG_DEBUG_HTTP, log, 0, fmt, arg1, arg2, arg3, arg4, arg5)
#define logHttpDebugMsg6(log, fmt, arg1, arg2, arg3, arg4, arg5, arg6)             ngx_log_debug6(NGX_LOG_DEBUG_HTTP, log, 0, fmt, arg1, arg2, arg3, arg4, arg5, arg6)
#define logHttpDebugMsg7(log, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7)       ngx_log_debug7(NGX_LOG_DEBUG_HTTP, log, 0, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define logHttpDebugMsg8(log, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) ngx_log_debug8(NGX_LOG_DEBUG_HTTP, log, 0, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)


enum {
    CounterMainMergeCall, CounterSrvMergeCall, CounterLocMergeCall,
    CounterMainCreateCall, CounterSrvCreateCall, CounterLocCreateCall,
    CounterHandlerInvoke, CounterPatternParse, CounterFileCount, CounterLimit
};

#define ngx_str_compare(a,b) ((a)->len == (b)->len && !ngx_memcmp((a)->data, (b)->data, (a)->len))
#define ngx_cstr_compare(a,b) ((a)->len == (sizeof(b) - 1) && !ngx_memcmp((a)->data, b, (a)->len))

typedef ngx_int_t ngx_rc_t;
ngx_rc_t ngx_http_autols_init(ngx_conf_t *cf);

void *ngx_http_autols_create_main_conf(ngx_conf_t *cf);
char *ngx_http_autols_init_main_conf(ngx_conf_t *cf, void *conf);

void *ngx_http_autols_create_loc_conf(ngx_conf_t *cf);
char *ngx_http_autols_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);

ngx_rc_t ngx_http_autols_handler(ngx_http_request_t *r);

char* ngx_conf_autols_set_regex_then_string_array_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char* ngx_conf_autols_set_keyval_array_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char* ngx_conf_autols_set_str_array_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

typedef struct {
	ngx_tm_t createdOn;
    ngx_array_t patterns;
} ngx_http_autols_main_conf_t;

typedef struct {
	ngx_tm_t createdOn;
	ngx_flag_t enable, printDebug, localTime;
    ngx_str_t patternPath;
    ngx_array_t *entryIgnores, *sections, *keyValuePairs;
} ngx_http_autols_loc_conf_t;


typedef struct { ngx_str_t name, value; } alsPatternAttribute;

typedef struct {
	ngx_array_t children, attributes;

	ngx_str_t name;
	char *start, *end;
} alsPatternToken;

typedef struct {
	ngx_str_t path, content;
    ngx_array_t tokens;
} alsPattern;


typedef struct {
    ngx_http_autols_main_conf_t *mainConf;
    ngx_http_autols_loc_conf_t *locConf;
    ngx_http_request_t *request;

    ngx_str_t requestPath;
	int32_t requestPathCapacity;
    int32_t ptnEntryStartPos;
} alsConnectionConfig;


typedef struct {
    ngx_array_t fileEntries;

	int64_t totalFileSize;

	int32_t totalFileNamesLength;
	int32_t totalFileNamesLengthUriEscaped;
    int32_t totalFileNamesLengthHtmlEscaped;
} alsFileEntriesInfo;

typedef struct {
    ngx_str_t name;

    unsigned isDirectory:1;
    ngx_tm_t modifiedOn;
	int64_t size;

	int32_t nameLenAsHtml, nameLenAsUri;
} alsFileEntry;

extern ngx_module_t ngx_http_autols_module;

#endif