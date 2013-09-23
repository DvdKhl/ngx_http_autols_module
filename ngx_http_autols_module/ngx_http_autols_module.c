#include "ngx_http_autols_module.h"

static ngx_rc_t setRequestPath(connectionConf_T *conConf) {
    size_t     root;
    u_char    *last;
    ngx_str_t *reqPath;

    last = ngx_http_map_uri_to_path(conConf->request, &conConf->requestPath, &root, STRING_PREALLOCATE);
    if(last == NULL) return NGX_HTTP_INTERNAL_SERVER_ERROR;
    reqPath = &conConf->requestPath;

    conConf->requestPathCapacity = reqPath->len;
    reqPath->len = last - reqPath->data;
    if(reqPath->len > 1) reqPath->len--;
    reqPath->data[reqPath->len] = '\0';

    logHttpDebugMsg1(conConf, "autols: Request Path \"%V\"", reqPath);
    return NGX_OK;
}

static ngx_rc_t openDirectory(connectionConf_T *conConf, ngx_dir_t *dir) {
    ngx_rc_t  rc;
    ngx_err_t  err;
    ngx_uint_t level;

    if(ngx_open_dir(&conConf->requestPath, dir) == NGX_ERROR) {
        err = ngx_errno;
        if(err == NGX_ENOENT || err == NGX_ENOTDIR || err == NGX_ENAMETOOLONG) {
            level = NGX_LOG_ERR;
            rc = NGX_HTTP_NOT_FOUND;

        } else if(err == NGX_EACCES) {
            level = NGX_LOG_ERR;
            rc = NGX_HTTP_FORBIDDEN;

        } else {
            level = NGX_LOG_CRIT;
            rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        ngx_log_error(level, conConf->log, err, ngx_open_dir_n " \"%V\" failed", &conConf->requestPath);

        return rc;
    }
    logHttpDebugMsg0(conConf, "autols: Directory opened");
    return NGX_OK;
}

static ngx_rc_t sendHeaders(connectionConf_T *conConf, ngx_dir_t *dir) {
    ngx_rc_t rc;

    conConf->request->headers_out.status = NGX_HTTP_OK;
    conConf->request->headers_out.content_type_len = sizeof("text/html") - 1;
    ngx_str_set(&conConf->request->headers_out.content_type, "text/html");

    rc = ngx_http_send_header(conConf->request);
    if(rc == NGX_ERROR || rc > NGX_OK || conConf->request->header_only) {
        if(ngx_close_dir(dir) == NGX_ERROR) {
            ngx_log_error(
                NGX_LOG_ALERT, conConf->log,
                ngx_errno, ngx_close_dir_n " \"%V\" failed", conConf->requestPath);
        }
        return rc;
    }

    logHttpDebugMsg0(conConf, "autols: Header sent");
    return NGX_OK;
}

static ngx_rc_t getFiles(connectionConf_T *conConf, ngx_dir_t *dir, fileEntriesInfo_T *fileEntriesInfo) {
    ngx_int_t    gmtOffset;
    ngx_str_t   *reqPath;
    fileEntry_t *entry;
    ngx_err_t    err;

    gmtOffset = (ngx_timeofday())->gmtoff * 60;
    reqPath   = &conConf->requestPath;

    fileEntriesInfo->totalFileNamesLength =
        fileEntriesInfo->totalFileNamesLengthUriEscaped =
        fileEntriesInfo->totalFileNamesLengthHtmlEscaped = 0;

    if(conConf->requestPathCapacity <= reqPath->len + 1) return NGX_ERROR;
    reqPath->data[reqPath->len++] = '/';
    reqPath->data[reqPath->len] = '\0';

    logHttpDebugMsg0(conConf, "autols: ##Iterating files");
    for(;;) {
        size_t fileNameLength;

        ngx_set_errno(0);
        if(ngx_read_dir(dir) == NGX_ERROR) {
            err = ngx_errno;

            if(err != NGX_ENOMOREFILES) {
                //logDirError(conConf->request, &conConf->dir, &path);

                ngx_log_error(
                    NGX_LOG_CRIT, conConf->log, err,
                    ngx_read_dir_n " \"%V\" failed", reqPath->data);

                return logDirError(conConf, dir, reqPath);
            }
            break;
        }

        logHttpDebugMsg1(conConf, "autols: #File \"%s\"", ngx_de_name(dir));

        fileNameLength = ngx_de_namelen(dir);
        fileEntriesInfo->totalFileNamesLength += fileNameLength;

        if(ngx_de_name(dir)[0] == '.' && ngx_de_name(dir)[1] == '\0') {
            logHttpDebugMsg0(conConf, "autols: Skipping filepath");
            continue;
        }
        //TODO: Filter entries based on user settings

        if(!dir->valid_info) {
            size_t reqPathCap;
            u_char *last;

            logHttpDebugMsg0(conConf, "autols: Checking if filepath is valid");

            reqPathCap = conConf->requestPathCapacity;
            last = reqPath->data + reqPath->len;

            //Building file path: include terminating '\0'
            if(reqPath->len + fileNameLength + 1 > reqPathCap) {
                u_char *filePath = reqPath->data;

                logHttpDebugMsg0(conConf, "autols: Increasing path capacity to store full filepath");
                reqPathCap = reqPath->len + fileNameLength + 1 + STRING_PREALLOCATE;

                filePath = (u_char*)ngx_pnalloc(conConf->pool, reqPathCap);
                if(filePath == NULL) return logDirError(conConf, dir, reqPath);

                last = ngx_cpystrn(filePath, reqPath->data, reqPath->len + 1);

                reqPath->data = filePath;
                conConf->requestPathCapacity = reqPathCap;
            }
            ngx_cpystrn(last, ngx_de_name(dir), fileNameLength + 1); //Including null termination
            logHttpDebugMsg2(conConf, "autols: Full path: \"%V%s\"", reqPath, ngx_de_name(dir));

            if(ngx_de_info(reqPath->data, dir) == NGX_FILE_ERROR) {
                err = ngx_errno;

                if(err != NGX_ENOENT && err != NGX_ELOOP) {
                    ngx_log_error(
                        NGX_LOG_CRIT, conConf->log, err,
                        "autols: " ngx_de_info_n " \"%V\" failed", reqPath);

                    if(err == NGX_EACCES) continue;
                    return logDirError(conConf, dir, reqPath);
                }

                if(ngx_de_link_info(reqPath->data, dir) == NGX_FILE_ERROR) {
                    ngx_log_error(
                        NGX_LOG_CRIT, conConf->log, ngx_errno,
                        "autols: " ngx_de_link_info_n " \"%V\" failed", reqPath);

                    return logDirError(conConf, dir, reqPath);
                }
            }
        }

        logHttpDebugMsg0(conConf, "autols: Retrieving file info");
        entry = (fileEntry_t*)ngx_array_push(&fileEntriesInfo->fileEntries);
        if(entry == NULL) return logDirError(conConf, dir, reqPath);

        entry->name.len = fileNameLength;
        entry->name.data = (u_char*)ngx_pnalloc(conConf->pool, fileNameLength + 1);
        if(entry->name.data == NULL) return logDirError(conConf, dir, reqPath);
        ngx_cpystrn(entry->name.data, ngx_de_name(dir), fileNameLength + 1);

        //With local time offset if specified
        ngx_gmtime(ngx_de_mtime(dir) + gmtOffset * conConf->mainConf->localTime, &entry->modifiedOn);
        entry->isDirectory = ngx_de_is_dir(dir);
        entry->size = ngx_de_size(dir);

        fileEntriesInfo->totalFileNamesLengthUriEscaped += entry->nameLenAsUri = fileNameLength +
            ngx_escape_uri(NULL, entry->name.data, entry->name.len, NGX_ESCAPE_URI_COMPONENT);

        fileEntriesInfo->totalFileNamesLengthHtmlEscaped += entry->nameLenAsHtml = fileNameLength +
            ngx_escape_html(NULL, entry->name.data, entry->name.len);
        logHttpDebugMsg0(conConf, "autols: File info retrieved");
    }

    return NGX_OK;
}

static ngx_rc_t closeDirectory(connectionConf_T *conConf, ngx_dir_t *dir) {
    if(ngx_close_dir(dir) == NGX_ERROR) {
        ngx_log_error(
            NGX_LOG_ALERT, conConf->log, ngx_errno,
            ngx_close_dir_n " \"%V\" failed", conConf->requestPath);
    }
    logHttpDebugMsg0(conConf, "autols: Directory closed");
    return NGX_OK;
}

static connectionConf_T *cc;

static ngx_rc_t createReplyBody(connectionConf_T *conConf, fileEntriesInfo_T *fileEntriesInfo, ngx_chain_t *out) {
    templateToken_t *token, *nextToken;
    ngx_array_t *tokens;
    size_t bufSize;
    int doAppend;
    strb_t strb;
    u_char *tpl;

    if(!parseTemplate(conConf)) return NGX_ERROR;

    tpl = conConf->mainConf->pageTemplate.data;

    bufSize =
        conConf->mainConf->pageTemplate.len +
        (
        conConf->mainConf->createBody +
        conConf->mainConf->createJsVariable
        ) * 256 * fileEntriesInfo->fileEntries.nelts;

    logHttpDebugMsg1(conConf, "autols: Estimated buffer size: %d", bufSize);

    if(!strbInit(&strb, conConf->request->pool, bufSize, bufSize)) return NGX_ERROR;

    tokens = &conConf->mainConf->pageTemplateTokens;
    token = (templateToken_t*)tokens->elts;
    nextToken = token;

    logHttpDebugMsg0(conConf, "autols: ##Applying template");
    for(;;) {
        token = nextToken++;

        logHttpDebugMsg1(conConf, "autols: #Processing Token: %V", &nextToken->name);

        if(!strbAppendMemory(&strb, tpl + token->endAt, nextToken->startAt - token->endAt)) return NGX_ERROR;

        if(compareTokenName(nextToken, &tplJsVariableStartStr)) {
            doAppend = conConf->mainConf->createJsVariable;
            nextToken = appendSection(nextToken, (doAppend ? &strb : NULL), tpl, &tplJsVariableEndStr, NULL, fileEntriesInfo);

        } else if(compareTokenName(nextToken, &tplJsSourceStartStr)) {
            doAppend = conConf->mainConf->jsSourcePath.len != 0;
            nextToken = appendSection(nextToken, (doAppend ? &strb : NULL), tpl, &tplJsSourceEndStr, NULL, fileEntriesInfo);

        } else if(compareTokenName(nextToken, &tplCssSourceStartStr)) {
            doAppend = conConf->mainConf->cssSourcePath.len != 0;
            nextToken = appendSection(nextToken, (doAppend ? &strb : NULL), tpl, &tplCssSourceEndStr, NULL, fileEntriesInfo);

        } else if(compareTokenName(nextToken, &tplBodyStartStr)) {
            doAppend = conConf->mainConf->createBody;
            nextToken = appendSection(nextToken, (doAppend ? &strb : NULL), tpl, &tplBodyEndStr, NULL, fileEntriesInfo);

        } else if(nextToken->name.data != NULL) {
            if(!appendGlobalTokenValue(nextToken, &strb, NULL)) return NGX_ERROR;
        }

        if(nextToken == NULL) return NGX_ERROR;
        if(nextToken->name.data == NULL) break;
    }
    logHttpDebugMsg0(conConf, "autols: Reply body generated");

    if(conConf->request == conConf->request->main) {
        strb.currentLink->buf->last_buf = 1;
        //TODO?: String builder cleanup
    }

    *out = *strb.firstLink;
    return NGX_OK;
}


static ngx_rc_t ngx_http_autols_handler(ngx_http_request_t *r) {
    fileEntriesInfo_T fileEntriesInfo;
    connectionConf_T  conConf;
    ngx_str_t        *reqPath;
    ngx_chain_t       out;
    ngx_dir_t         dir;
    ngx_rc_t          rc;

    handlerInvokeCount++;
    cc = &conConf; //Debugging in methods with no config param TODO: Remove for release

    if(r->uri.data[r->uri.len - 1] != '/') return NGX_DECLINED;
    if(!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) return NGX_DECLINED;

    //Get Settings
    conConf.mainConf = (ngx_http_autols_loc_conf_t*)ngx_http_get_module_loc_conf(r, ngx_http_autols_module);
    conConf.log = r->connection->log;
    conConf.request = r;

    //Check if we're enabled
    if(!conConf.mainConf->enable) {
        logHttpDebugMsg0(&conConf, "autols: Declined request");
        return NGX_DECLINED;
    }
    logHttpDebugMsg0(&conConf, "autols: Accepted request");
    logHttpDebugMsg2(&conConf, "autols: MergeCallCount=%d HandlerInvokeCount=%d", mergeCallCount, handlerInvokeCount);

    //Get Request Path
    rc = setRequestPath(&conConf);
    if(rc != NGX_OK) return rc;
    reqPath = &conConf.requestPath;

    //Open Directory
    rc = openDirectory(&conConf, &dir);
    if(rc != NGX_OK) return rc;

    //Send Header
    rc = sendHeaders(&conConf, &dir);
    if(rc != NGX_OK) return rc;

    //Create local pool and initialize file entries array
    conConf.pool = ngx_create_pool((sizeof(fileEntry_t) + 20) * 400, conConf.log); //TODO: Constant
    if(ngx_array_init(&fileEntriesInfo.fileEntries, conConf.pool, 40, sizeof(fileEntry_t)) != NGX_OK) {
        return logDirError(&conConf, &dir, reqPath);
    }

    //Get File Entries
    rc = getFiles(&conConf, &dir, &fileEntriesInfo);
    if(rc != NGX_OK) return rc;

    //Close Directory
    rc = closeDirectory(&conConf, &dir);
    if(rc != NGX_OK) return rc;

    //Sort File Entries
    if(fileEntriesInfo.fileEntries.nelts > 1) {
        logHttpDebugMsg1(&conConf, "autols: Sorting %d file entries", fileEntriesInfo.fileEntries.nelts);
        ngx_qsort(
            fileEntriesInfo.fileEntries.elts,
            (size_t)fileEntriesInfo.fileEntries.nelts,
            sizeof(fileEntry_t),
            fileEntryComparer);
    }


    //Create Reply Body
    out.buf = NULL; out.next = NULL;
    rc = createReplyBody(&conConf, &fileEntriesInfo, &out);
    if(rc != NGX_OK) return rc;

    //Release Local Pool
    ngx_destroy_pool(conConf.pool);

    //Hand off reply body to filters and return their return code
    return ngx_http_output_filter(r, &out);
}

static void* ngx_http_autols_create_loc_conf(ngx_conf_t *cf) {
    ngx_http_autols_loc_conf_t  *conf;

    conf = (ngx_http_autols_loc_conf_t*)ngx_pcalloc(cf->pool, sizeof(ngx_http_autols_loc_conf_t));
    if(conf == NULL) return NULL;

    conf->enable = NGX_CONF_UNSET;
    conf->createJsVariable = NGX_CONF_UNSET;
    conf->createBody = NGX_CONF_UNSET;
    conf->localTime = NGX_CONF_UNSET;

    return conf;
}

static char* ngx_http_autols_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child) {
    ngx_http_autols_loc_conf_t *prev = (ngx_http_autols_loc_conf_t*)parent;
    ngx_http_autols_loc_conf_t *conf = (ngx_http_autols_loc_conf_t*)child;


    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_value(conf->createJsVariable, prev->createJsVariable, 0);
    ngx_conf_merge_value(conf->createBody, prev->createBody, 1);
    ngx_conf_merge_value(conf->localTime, prev->localTime, 0);

    ngx_conf_merge_str_value(conf->charSet, prev->charSet, "");
    ngx_conf_merge_str_value(conf->jsSourcePath, prev->jsSourcePath, "");
    ngx_conf_merge_str_value(conf->cssSourcePath, prev->cssSourcePath, "");
    ngx_conf_merge_str_value(conf->pageTemplatePath, prev->pageTemplatePath, "");

    mergeCallCount++;

    return NGX_CONF_OK;
}

static ngx_rc_t ngx_http_autols_init(ngx_conf_t *cf) {
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = (ngx_http_core_main_conf_t*)ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = (ngx_http_handler_pt*)ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    if(h == NULL) return NGX_ERROR;

    *h = ngx_http_autols_handler;

    return NGX_OK;
}


static ngx_rc_t logDirError(connectionConf_T *conConf, ngx_dir_t *dir, ngx_str_t *name) {
    if(ngx_close_dir(dir) == NGX_ERROR) {
        ngx_log_error(
            NGX_LOG_ALERT, conConf->log, ngx_errno,
            ngx_close_dir_n " \"%V\" failed", name);
    }

    return conConf->request->header_sent ? NGX_ERROR : NGX_HTTP_INTERNAL_SERVER_ERROR;
}

static int ngx_libc_cdecl fileEntryComparer(const void *one, const void *two) {
    fileEntry_t *first = (fileEntry_t*)one;
    fileEntry_t *second = (fileEntry_t*)two;

    if(first->isDirectory && !second->isDirectory) return -1; //move the directories to the start
    if(!first->isDirectory && second->isDirectory) return 1; //move the directories to the start

    return (int) ngx_strcmp(first->name.data, second->name.data);
}


static int compareTokenName(templateToken_t *token, ngx_str_t *name) {
    return name->len == token->name.len && !ngx_memcmp(name->data, token->name.data, token->name.len);
}


static int parseTemplate(connectionConf_T *conConf) {
    u_char *tpl, *tplLast, *tokenNameStart, *attributeNameStart, *attributeValueStart;
    size_t tokenNameLength, attributeNameLength, attributeValueLength;
    templateTokenAttribute_t *attribute;
    templateToken_t *token;

    if(conConf->mainConf->pageTemplate.len == 0) {
        logHttpDebugMsg0(conConf, "autols: No template set, using default");
        conConf->mainConf->pageTemplate.data = defaultPageTemplate;
        conConf->mainConf->pageTemplate.len = ngx_strlen(defaultPageTemplate);
    }
    tpl = conConf->mainConf->pageTemplate.data;

    if(ngx_array_init(&conConf->mainConf->pageTemplateTokens, conConf->pool, 0, sizeof(templateToken_t)) != NGX_OK) return 0;


    //Start Token
    token = (templateToken_t*)ngx_array_push(&conConf->mainConf->pageTemplateTokens);
    if(token == NULL) return 0;

    token->startAt = token->endAt = 0;
    token->name.data = NULL;
    token->name.len = 0;

    tplLast = tpl + conConf->mainConf->pageTemplate.len;

    logHttpDebugMsg0(conConf, "autols: Parsing template");
    for(;tpl < tplLast - 3; tpl++) {
        if(tpl[0] != '&' || tpl[1] != '{') continue;
        logHttpDebugMsg1(conConf, "autols: #Start of tag found at %d", tpl - conConf->mainConf->pageTemplate.data);

        token = (templateToken_t*)ngx_array_push(&conConf->mainConf->pageTemplateTokens);
        if(token == NULL) return 0;

        tokenNameStart = (tpl += 2);
        while(((*tpl >= 'A' && *tpl <= 'Z') || (*tpl >= 'a' && *tpl <= 'z') || (*tpl >= '0' && *tpl <= '9')) && ++tpl < tplLast) ;
        tokenNameLength = tpl - tokenNameStart;

        logHttpDebugMsg1(conConf, "autols: Token length is %d", tokenNameLength);

        if(tokenNameLength == 0 || tpl >= tplLast || (*tpl != '}' && *tpl != '?')) return 0;

        if(ngx_array_init(&token->attributes, conConf->pool, 0, sizeof(templateTokenAttribute_t)) != NGX_OK) return 0;

        while(*tpl == '?' || *tpl == '&') {
            logHttpDebugMsg1(conConf, "autols: Start of attribute found at %d", tpl - conConf->mainConf->pageTemplate.data);

            attributeNameStart = ++tpl;
            while(((*tpl >= 'A' && *tpl <= 'Z') || (*tpl >= 'a' && *tpl <= 'z') || (*tpl >= '0' && *tpl <= '9')) && ++tpl < tplLast) ;
            attributeNameLength = tpl - attributeNameStart;
            if(attributeNameLength == 0 || tpl >= tplLast || *tpl != '=') return 0;

            attributeValueStart = ++tpl;
            while(((*tpl >= 'A' && *tpl <= 'Z') || (*tpl >= 'a' && *tpl <= 'z') || (*tpl >= '0' && *tpl <= '9')) && ++tpl < tplLast) ;
            attributeValueLength = tpl - attributeValueStart;
            if(attributeValueLength == 0 || tpl >= tplLast || (*tpl != '}' && *tpl != '&')) return 0;

            attribute = (templateTokenAttribute_t*)ngx_array_push(&token->attributes);
            if(attribute == NULL) return 0;

            attribute->name.data = attributeNameStart;
            attribute->name.len = attributeNameLength;
            attribute->value.data = attributeValueStart;
            attribute->value.len = attributeValueLength;
            logHttpDebugMsg2(conConf, "autols: Attribute information set (%V=\"%V\")", &attribute->name, &attribute->value);
        }

        token->name.data = tokenNameStart;
        token->name.len = tokenNameLength;
        token->startAt = (tokenNameStart - conConf->mainConf->pageTemplate.data) - 2;
        token->endAt = (tpl - conConf->mainConf->pageTemplate.data) + 1;
        logHttpDebugMsg4(conConf, "autols: Token information set (%V:%d, %d-%d)", &token->name, tokenNameLength, token->startAt, token->endAt);
    }

    //End Token
    token = (templateToken_t*)ngx_array_push(&conConf->mainConf->pageTemplateTokens);
    if(token == NULL) return 0;

    token->startAt = token->endAt = conConf->mainConf->pageTemplate.len;
    token->name.data = NULL;
    token->name.len = 0;

    logHttpDebugMsg1(conConf, "autols: Template parsed. %d Tokens found", conConf->mainConf->pageTemplateTokens.nelts);

    return 1;
}

static int appendGlobalTokenValue(templateToken_t *token, strb_t *strb, void *globalInfo) {
    if(!strb) return 1;

    logHttpDebugMsg1(cc, "autols: Appending Global TokenValue \"%V\"", &token->name);
    return 1;
}

static int appendEntryTokenValue(templateToken_t *token, strb_t *strb, fileEntry_t *fileEntry) {
    if(!strb) return 1;

    logHttpDebugMsg1(cc, "autols: Appending Entry TokenValue \"%V\"", &token->name);

    if(compareTokenName(token, &tplEntryIsDirectoryStr)) {
        if(!strbAppendSingle(strb, fileEntry->isDirectory ? '1' : '0')) return 0;

    } else if(compareTokenName(token, &tplEntryModifiedOnStr)) {
        ngx_tm_t tm = fileEntry->modifiedOn;
        if(!strbFormat(strb, "%02d-%02d-%d %02d:%02d",
            tm.ngx_tm_mday, tm.ngx_tm_mon, tm.ngx_tm_year,
            tm.ngx_tm_hour, tm.ngx_tm_min)) return 0;

    } else if(compareTokenName(token, &tplEntrySizeStr)) {
        if(!strbFormat(strb, "%d", fileEntry->size)) return 0;

    } else if(compareTokenName(token, &tplEntryNameStr)) {
        if(!strbAppendNgxString(strb, &fileEntry->name)) return 0;
    }

    return 1;
}

static templateToken_t* appendSection(templateToken_t *token, strb_t *strb, u_char *tpl, ngx_str_t *endTokenName, void *globalInfo, fileEntriesInfo_T *fileEntriesInfo) {
    templateToken_t *nextToken, *entryStartToken;
    fileEntry_t *fileEntry, *lastFileEntry;

    logHttpDebugMsg0(cc, "autols: Section Start");

    //Single non-entries section or beginning of entries section
    nextToken = token;
    for(;;) {
        token = nextToken++;

        if(strb) if(!strbAppendMemory(strb, tpl + token->endAt, nextToken->startAt - token->endAt)) return NULL;

        if(nextToken->name.data == NULL ||
            compareTokenName(nextToken, &tplEntryStartStr) ||
            compareTokenName(nextToken, endTokenName)) break;

        if(!appendGlobalTokenValue(nextToken, strb, globalInfo)) return NULL;
    }

    if(nextToken->name.data == NULL) return NULL;
    if(compareTokenName(nextToken, endTokenName)) {
        logHttpDebugMsg0(cc, "autols: Section End");
        return nextToken;
    }

    //If we reach here we found "EntryStart" token, bail out if no file entries are available
    if(fileEntriesInfo == NULL) return NULL;

    //FileEntries part of section
    entryStartToken = nextToken;
    fileEntry = (fileEntry_t*)fileEntriesInfo->fileEntries.elts;
    lastFileEntry = fileEntry + fileEntriesInfo->fileEntries.nelts;
    while(fileEntry != lastFileEntry) {
        nextToken = entryStartToken;

        for(;;) {
            token = nextToken++;
             if(strb) if(!strbAppendMemory(strb, tpl + token->endAt, nextToken->startAt - token->endAt)) return NULL;

            if(nextToken->name.data == NULL || compareTokenName(nextToken, &tplEntryEndStr)) break;
            if(!appendEntryTokenValue(nextToken, strb, fileEntry)) return NULL;
        }
        if(nextToken->name.data == NULL) return NULL;
        fileEntry++;
        if(strb) if(!strbAppendCString(strb, (u_char*)CRLF)) return 0;
    }
    strbDecreaseSizeBy(strb, 2);

    //Post FileEntries part of section
    for(;;) {
        token = nextToken++;
        if(strb) if(!strbAppendMemory(strb, tpl + token->endAt, nextToken->startAt - token->endAt)) return NULL;

        if(nextToken->name.data == NULL || compareTokenName(nextToken, endTokenName)) break;
        if(!appendGlobalTokenValue(nextToken, strb, globalInfo)) return NULL;
    }

    logHttpDebugMsg0(cc, "autols: Section End");

    return nextToken;
}
