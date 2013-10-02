#pragma once

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "strb.h"

#if NGX_PCRE
static int hasRegex = 1;
#else
static int hasRegex = 0;
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

static ngx_str_t tplReplyCharSetStr = ngx_string("ReplyCharSet");
static ngx_str_t tplRequestUriStr = ngx_string("RequestUri");

static ngx_str_t tplJsVariableStartStr = ngx_string("JSVariableStart");
static ngx_str_t tplJsVariableEndStr = ngx_string("JSVariableEnd");

static ngx_str_t tplJsSourceStartStr = ngx_string("JSSourceStart");
static ngx_str_t tplJsSourceEndStr = ngx_string("JSSourceEnd");

static ngx_str_t tplCssSourceStartStr = ngx_string("CSSSourceStart");
static ngx_str_t tplCssSourceEndStr = ngx_string("CSSSourceEnd");

static ngx_str_t tplBodyStartStr = ngx_string("BodyStart");
static ngx_str_t tplBodyEndStr = ngx_string("BodyEnd");

static ngx_str_t tplEntryStartStr = ngx_string("EntryStart");
static ngx_str_t tplEntryIsDirectoryStr = ngx_string("EntryIsDirectory");
static ngx_str_t tplEntryModifiedOnStr = ngx_string("EntryModifiedOn");
static ngx_str_t tplEntrySizeStr = ngx_string("EntrySize");
static ngx_str_t tplEntryNameStr = ngx_string("EntryName");
static ngx_str_t tplEntryEndStr = ngx_string("EntryEnd");


static ngx_str_t tplAttNoCountStr = ngx_string("NoCount");
static ngx_str_t tplAttStartAtStr = ngx_string("StartAt");
static ngx_str_t tplAttEscapeStr = ngx_string("Escape");
static ngx_str_t tplAttUriComponentStr = ngx_string("UriComponent");
static ngx_str_t tplAttHttpStr = ngx_string("Http");
static ngx_str_t tplAttUriStr = ngx_string("Uri");
static ngx_str_t tplAttFormatStr = ngx_string("Format");
static ngx_str_t tplAttMaxLengthStr = ngx_string("MaxLength");

static u_char defaultPagePattern[] =
    "<!DOCTYPE html>" CRLF
    "<html>" CRLF
    "  <head>" CRLF
    "    <meta charset=\"&{ReplyCharSet}\">" CRLF
    "    <title>Index of &{RequestUri}</title>&{JSVariableStart}" CRLF
    "    <script type=\"text/javascript\">" CRLF
    "      var dirListing = [&{EntryStart}" CRLF
    "        {" CRLF
    "          \"isDirectory\": &{EntryIsDirectory}," CRLF
    "          \"modifiedOn\": \"&{EntryModifiedOn}\"," CRLF
    "          \"size\": &{EntrySize}," CRLF
    "          \"name\": \"&{EntryName}\"" CRLF
    "        },&{EntryEnd}" CRLF
    "      ]" CRLF
    "    </script>&{JSVariableEnd}&{JSSourceStart}" CRLF
    "    <script type=\"text/javascript\" src=\"&{JSSource}\"></script>&{JSSourceEnd}&{CSSSourceStart}" CRLF
    "    <link rel=\"stylesheet\" type=\"text/css\" href=\"&{CSSSource}\">&{CSSSourceEnd}" CRLF
    "  </head>" CRLF
    "  <body bgcolor=\"white\">&{BodyStart}" CRLF
    "    <h1>Index of &{RequestUri}</h1>" CRLF
    "    <hr>" CRLF
    "    <pre>&{EntryStart}<a href=\"&{EntryName?Escape=Uri&NoCount}\">&{EntryName?MaxLength=66}</a>&{EntryModifiedOn?StartAt=82} &{EntrySize?Format=%24s}&{EntryEnd}</pre>&{BodyEnd}" CRLF
    "  </body>" CRLF
    "</html>";

enum {
    CounterMainMergeCall, CounterSrvMergeCall, CounterLocMergeCall,
    CounterMainCreateCall, CounterSrvCreateCall, CounterLocCreateCall,
    CounterHandlerInvoke, CounterPatternParse, CounterFileCount, CounterLimit
};
static const char *counterNames[] = {
    "MainMergeCall","SrvMergeCall","LocMergeCall",
    "MainCreateCall","SrvCreateCall","LocCreateCall",
    "HandlerInvoke", "TemplateParse", "FileCount"
};
static int counters[CounterLimit];

#define ngx_str_compare(a,b) ((a)->len == (b)->len && !ngx_memcmp((a)->data, (b)->data, (a)->len))

typedef ngx_int_t ngx_rc_t;
static ngx_rc_t ngx_http_autols_init(ngx_conf_t *cf);

static void *ngx_http_autols_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_autols_init_main_conf(ngx_conf_t *cf, void *conf);

static void *ngx_http_autols_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_autols_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);

static char* ngx_conf_autols_regex_then_string_array_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

typedef struct { ngx_str_t name, value; } patternAttribute_t;

typedef struct {
    ngx_str_t name;
    size_t startAt, endAt;
    ngx_array_t attributes;
} patternToken_t;

typedef struct {
    ngx_str_t path;

    ngx_str_t content;
    ngx_array_t tokens;
} pattern_t;


typedef struct {
    ngx_pool_t *pool;

    ngx_array_t patterns;
} ngx_http_autols_main_conf_t;

typedef struct {
    ngx_flag_t enable, createJsVariable, createBody, localTime;
    ngx_str_t charSet, jsSourcePath, cssSourcePath, pagePatternPath;
    ngx_array_t *entryIgnores;
} ngx_http_autols_loc_conf_t;

typedef struct {
    ngx_http_autols_main_conf_t *mainConf;
    ngx_http_autols_loc_conf_t *locConf;
    ngx_http_request_t *request;
    ngx_pool_t *pool;
    ngx_log_t *log;

    ngx_str_t requestPath;
    size_t requestPathCapacity;

    int32_t tplEntryStartPos;
} conConf_t;


typedef struct {
    ngx_array_t fileEntries;

    size_t totalFileSize;

    size_t totalFileNamesLength;
    size_t totalFileNamesLengthUriEscaped;
    size_t totalFileNamesLengthHtmlEscaped;
} fileEntriesInfo_T;

typedef struct {
    ngx_str_t      name;
    size_t         nameLenAsHtml, nameLenAsUri;
    unsigned       isDirectory:1;
    ngx_tm_t       modifiedOn;
    off_t          size;
} fileEntry_t;

static ngx_command_t ngx_http_autols_commands[] = {
    { ngx_string("autols"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG, //TODO: NGX_CONF_FLAG?
    ngx_conf_set_flag_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_autols_loc_conf_t, enable),
    NULL },

    { ngx_string("autols_ignore"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE, //TODO: NGX_CONF_FLAG?
    ngx_conf_autols_regex_then_string_array_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_autols_loc_conf_t, entryIgnores),
    NULL },

    { ngx_string("autols_create_js_variable"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG, //TODO: NGX_CONF_FLAG?
    ngx_conf_set_flag_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_autols_loc_conf_t, createJsVariable),
    NULL },

    { ngx_string("autols_create_body"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG, //TODO: NGX_CONF_FLAG?
    ngx_conf_set_flag_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_autols_loc_conf_t, createBody),
    NULL },

    { ngx_string("autols_local_time"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG, //TODO: NGX_CONF_FLAG?
    ngx_conf_set_flag_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_autols_loc_conf_t, localTime),
    NULL },

    { ngx_string("autols_charset"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1, //TODO: NGX_CONF_FLAG?
    ngx_conf_set_flag_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_autols_loc_conf_t, charSet),
    NULL },

    { ngx_string("autols_js_source_path"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1, //TODO: NGX_CONF_FLAG?
    ngx_conf_set_str_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_autols_loc_conf_t, jsSourcePath),
    NULL },

    { ngx_string("autols_css_source_path"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1, //TODO: NGX_CONF_FLAG?
    ngx_conf_set_str_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_autols_loc_conf_t, cssSourcePath),
    NULL },

    { ngx_string("autols_page_template_path"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1, //TODO: NGX_CONF_FLAG?
    ngx_conf_set_str_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_autols_loc_conf_t, pagePatternPath),
    NULL },

    ngx_null_command
};

static ngx_http_module_t  ngx_http_autols_module_ctx = {
    NULL,                              /* preconfiguration */
    ngx_http_autols_init,              /* postconfiguration */

    ngx_http_autols_create_main_conf,  /* create main configuration */
    ngx_http_autols_init_main_conf,   /* init main configuration */

    NULL,                              /* create server configuration */
    NULL,                              /* merge server configuration */

    ngx_http_autols_create_loc_conf,   /* create location configuration */
    ngx_http_autols_merge_loc_conf     /* merge location configuration */
};

ngx_module_t  ngx_http_autols_module = {
    NGX_MODULE_V1,
    &ngx_http_autols_module_ctx,       /* module context */
    ngx_http_autols_commands,          /* module directives */
    NGX_HTTP_MODULE,                   /* module type */
    NULL,                              /* init master */
    NULL,                              /* init module */
    NULL,                              /* init process */
    NULL,                              /* init thread */
    NULL,                              /* exit thread */
    NULL,                              /* exit process */
    NULL,                              /* exit master */
    NGX_MODULE_V1_PADDING
};
