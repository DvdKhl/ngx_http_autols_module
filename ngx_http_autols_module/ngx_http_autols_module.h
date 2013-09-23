#pragma once

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "strb.h"


#define STRING_PREALLOCATE  50
#if STRING_PREALLOCATE < 1
#error STRING_PREALLOCATE must at least be higher than 0
#endif

#define logHttpDebugMsg0(conConf, fmt)                                                 ngx_log_debug0(NGX_LOG_DEBUG_HTTP, (conConf)->log, 0, fmt)
#define logHttpDebugMsg1(conConf, fmt, arg1)                                           ngx_log_debug1(NGX_LOG_DEBUG_HTTP, (conConf)->log, 0, fmt, arg1)
#define logHttpDebugMsg2(conConf, fmt, arg1, arg2)                                     ngx_log_debug2(NGX_LOG_DEBUG_HTTP, (conConf)->log, 0, fmt, arg1, arg2)
#define logHttpDebugMsg3(conConf, fmt, arg1, arg2, arg3)                               ngx_log_debug3(NGX_LOG_DEBUG_HTTP, (conConf)->log, 0, fmt, arg1, arg2, arg3)
#define logHttpDebugMsg4(conConf, fmt, arg1, arg2, arg3, arg4)                         ngx_log_debug4(NGX_LOG_DEBUG_HTTP, (conConf)->log, 0, fmt, arg1, arg2, arg3, arg4)
#define logHttpDebugMsg5(conConf, fmt, arg1, arg2, arg3, arg4, arg5)                   ngx_log_debug5(NGX_LOG_DEBUG_HTTP, (conConf)->log, 0, fmt, arg1, arg2, arg3, arg4, arg5)
#define logHttpDebugMsg6(conConf, fmt, arg1, arg2, arg3, arg4, arg5, arg6)             ngx_log_debug6(NGX_LOG_DEBUG_HTTP, (conConf)->log, 0, fmt, arg1, arg2, arg3, arg4, arg5, arg6)
#define logHttpDebugMsg7(conConf, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7)       ngx_log_debug7(NGX_LOG_DEBUG_HTTP, (conConf)->log, 0, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define logHttpDebugMsg8(conConf, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) ngx_log_debug8(NGX_LOG_DEBUG_HTTP, (conConf)->log, 0, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)

//static ngx_str_t tplReplyCharSetStr = ngx_string("ReplyCharSet");
//static ngx_str_t tplRequestUriStr = ngx_string("RequestUri");

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

static u_char defaultPageTemplate[] =
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
    "    <pre>&{EntryStart}<a href=\"&{EntryName?Escape=UriComponent}\">&{EntryName}</a>&{EntryModifiedOn?StartAt=52}&{EntrySize?StartAt=70}&{EntryEnd}</pre>&{BodyEnd}" CRLF
    "  </body>" CRLF
    "</html>";

static int mergeCallCount = 0, handlerInvokeCount = 0;

typedef ngx_int_t ngx_rc_t;

static ngx_rc_t ngx_http_autols_init(ngx_conf_t *cf);
static void *ngx_http_autols_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_autols_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);

typedef struct { ngx_str_t name, value; } templateTokenAttribute_t;

typedef struct {
    ngx_str_t name;
    size_t startAt, endAt;
    ngx_array_t attributes;
} templateToken_t;

typedef struct {
    ngx_flag_t enable, createJsVariable, createBody, localTime;
    ngx_str_t charSet, jsSourcePath, cssSourcePath, pageTemplatePath;
    ngx_str_t pageTemplate;
    ngx_array_t pageTemplateTokens;
} ngx_http_autols_loc_conf_t;

typedef struct {
    ngx_http_autols_loc_conf_t *mainConf;
    ngx_http_request_t *request;
    ngx_pool_t *pool;
    ngx_log_t *log;

    ngx_str_t requestPath;
    size_t requestPathCapacity;
} connectionConf_T;

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

static int ngx_libc_cdecl fileEntryComparer(const void *one, const void *two);
static ngx_rc_t logDirError(connectionConf_T *conConf, ngx_dir_t *dir, ngx_str_t *name);
static int compareTokenName(templateToken_t *token, ngx_str_t *name);

static int appendGlobalTokenValue(templateToken_t *token, strb_t *strb, void *globalInfo);
static templateToken_t* appendSection(templateToken_t *token, strb_t *strb, u_char *tpl, ngx_str_t *endTokenName, void *globalInfo, fileEntriesInfo_T *fileEntriesInfo);
static int parseTemplate(connectionConf_T *conConf);

static ngx_command_t ngx_http_autols_commands[] = {
    { ngx_string("autols"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG, //TODO: NGX_CONF_FLAG?
    ngx_conf_set_flag_slot, //TODO: Custom function
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_autols_loc_conf_t, enable),
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
    offsetof(ngx_http_autols_loc_conf_t, pageTemplatePath),
    NULL },

    ngx_null_command
};

static ngx_http_module_t  ngx_http_autols_module_ctx = {
    NULL,                              /* preconfiguration */
    ngx_http_autols_init,              /* postconfiguration */

    NULL,                              /* create main configuration */
    NULL,                              /* init main configuration */

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
