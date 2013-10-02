#include "ngx_http_autols_module.h"

static void appendConfig(strb_t *strb, conConf_t *conConf) {
    pattern_t *pattern, *patternLimit;
    patternToken_t *token, *tokenLimit;
    int i;

    strbFormat(strb, CRLF "locConf->enable = %d" CRLF, conConf->locConf->enable);
    strbFormat(strb, "locConf->createJsVariable = %d" CRLF, conConf->locConf->createJsVariable);
    strbFormat(strb, "locConf->createBody = %d" CRLF, conConf->locConf->createBody);
    strbFormat(strb, "locConf->localTime = %d" CRLF, conConf->locConf->localTime);
    strbFormat(strb, "locConf->charSet = %V" CRLF, &conConf->locConf->charSet);
    strbFormat(strb, "locConf->jsSourcePath = %V" CRLF, &conConf->locConf->jsSourcePath);
    strbFormat(strb, "locConf->cssSourcePath = %V" CRLF, &conConf->locConf->cssSourcePath);
    strbFormat(strb, "locConf->pagePatternPath = %V" CRLF, &conConf->locConf->pagePatternPath);

    //strbFormat(strb, "pattern->content = %s" CRLF, &conConf->pattern->content);
    //strbFormat(strb, "pattern->tokens.nelts = %d" CRLF, conConf->pattern->tokens.nelts);
    strbFormat(strb, "request->args = %V" CRLF, &conConf->request->args);
    strbFormat(strb, "request->headers_in.user_agent = %V" CRLF, &conConf->request->headers_in.user_agent->value);
    strbFormat(strb, "request->requestPath = %V" CRLF, &conConf->requestPath);
    strbFormat(strb, "request->requestPathCapacity = %d" CRLF, conConf->requestPathCapacity);
    strbFormat(strb, "request->tplEntryStartPos = %D" CRLF, conConf->tplEntryStartPos);

    for(i = 0; i < CounterLimit; i++) strbFormat(strb, "%s = %D" CRLF, counterNames[i], counters[i]);



    pattern = (pattern_t*)conConf->mainConf->patterns.elts;
    patternLimit = pattern + conConf->mainConf->patterns.nelts;
    while(pattern != patternLimit) {
        strbFormat(strb, "Path=%V ", &pattern->path);

        token = (patternToken_t*)pattern->tokens.elts;
        tokenLimit = token + pattern->tokens.nelts;
        while(token != tokenLimit) {
            strbFormat(strb, "(N=%V, S=%d, E=%d, AC=%d) ", &token->name, token->startAt, token->endAt, token->attributes.nelts);
            token++;
        }
        strbAppendCString(strb, CRLF);
        pattern++;
    }
    strbAppendCString(strb, CRLF CRLF);
}

static int ngx_libc_cdecl fileEntryComparer(const void *one, const void *two) {
    fileEntry_t *first = (fileEntry_t*)one;
    fileEntry_t *second = (fileEntry_t*)two;

    if(first->isDirectory && !second->isDirectory) return -1; //move the directories to the start
    if(!first->isDirectory && second->isDirectory) return 1; //move the directories to the start

    return (int) ngx_strcmp(first->name.data, second->name.data);
}


static int filterFileExactMatch(ngx_str_t *path, ngx_array_t *filters, ngx_log_t *log) {
    ngx_str_t *filter, *filterLast;
    int hasMatch = 0;

    if(filters == NULL) return 0;

    filter = (ngx_str_t*)filters->elts;
    filterLast = filter + filters->nelts;
    for(;filter != filterLast; filter++) {
        if(ngx_str_compare(filter, path)) {
            hasMatch = 1;
            break;
        }
    }

    return hasMatch;
}
static int filterFile(ngx_str_t *path, ngx_array_t *filters, ngx_log_t *log) {
    if(filters == NULL) return 0;
#if NGX_PCRE
    return ngx_regex_exec_array(filters, path, log) == NGX_OK;
#else
    return filterFileExactMatch(path, filters, log);
#endif
}


static int appendFileName(conConf_t *conConf, ngx_dir_t *dir, ngx_str_t *dst, size_t *dstCap) {
    size_t fileNameLength;
    ngx_err_t err;
    u_char *last;

    if(dir->valid_info) return NGX_OK;

    fileNameLength = ngx_de_namelen(dir);
    last = dst->data + dst->len;

    //Building file path: include terminating '\0'
    logHttpDebugMsg0(conConf->log, "autols: Checking if filepath is valid");
    if(dst->len + fileNameLength + 1 > *dstCap) {
        u_char *filePath = dst->data;

        logHttpDebugMsg0(conConf->log, "autols: Increasing path capacity to store full filepath");
        *dstCap = dst->len + fileNameLength + 1 + STRING_PREALLOCATE;

        filePath = (u_char*)ngx_pnalloc(conConf->pool, *dstCap);
        if(filePath == NULL) return 0;

        last = ngx_cpystrn(filePath, dst->data, dst->len + 1);

        dst->data = filePath;
    }
    last = ngx_cpystrn(last, ngx_de_name(dir), fileNameLength + 1);
    dst->len += fileNameLength;

    return 1;
}

static int parsePattern(pattern_t *pattern, ngx_pool_t *pool, ngx_log_t *log) {
    u_char *ptn, *ptnLast, *tokenNameStart, *attributeNameStart, *attributeValueStart;
    size_t tokenNameLength, attributeNameLength, attributeValueLength;
    patternAttribute_t *attribute;
    patternToken_t *token;

    counters[CounterPatternParse]++;

    if(ngx_array_init(&pattern->tokens, pool, 20, sizeof(patternToken_t)) != NGX_OK) return 0;

    //Start Token
    token = (patternToken_t*)ngx_array_push(&pattern->tokens);
    if(token == NULL) return 0;

    token->startAt = token->endAt = 0;
    token->name.data = NULL;
    token->name.len = 0;

    ptn = pattern->content.data;
    ptnLast = ptn + pattern->content.len;

    logHttpDebugMsg0(log, "autols: Parsing template");
    for(;ptn < ptnLast - 3; ptn++) {
        if(ptn[0] != '&' || ptn[1] != '{') continue;
        logHttpDebugMsg1(log, "autols: #Start of tag found at %d", ptn - pattern->content.data);

        token = (patternToken_t*)ngx_array_push(&pattern->tokens);
        if(token == NULL) return 0;

        tokenNameStart = (ptn += 2);
        while(((*ptn >= 'A' && *ptn <= 'Z') || (*ptn >= 'a' && *ptn <= 'z') || (*ptn >= '0' && *ptn <= '9')) && ++ptn < ptnLast) ;
        tokenNameLength = ptn - tokenNameStart;

        logHttpDebugMsg1(log, "autols: Token length is %d", tokenNameLength);

        if(tokenNameLength == 0 || ptn >= ptnLast || (*ptn != '}' && *ptn != '?')) return 0;

        if(ngx_array_init(&token->attributes, pool, 2, sizeof(patternAttribute_t)) != NGX_OK) return 0;

        while(*ptn == '?' || *ptn == '&') {
            logHttpDebugMsg1(log, "autols: Start of attribute found at %d", ptn - pattern->content.data);

            attributeNameStart = ++ptn;
            while(((*ptn >= 'A' && *ptn <= 'Z') || (*ptn >= 'a' && *ptn <= 'z') || (*ptn >= '0' && *ptn <= '9')) && ++ptn < ptnLast) ;
            attributeNameLength = ptn - attributeNameStart;
            if(attributeNameLength == 0 || ptn >= ptnLast || (*ptn != '=' && *ptn != '}' && *ptn != '&')) return 0;

            if(*ptn == '=') {
                attributeValueStart = ++ptn;
                while((*ptn != '}' && *ptn != '&') && ++ptn < ptnLast) ;
                attributeValueLength = ptn - attributeValueStart;
                if(attributeValueLength == 0 || ptn >= ptnLast) return 0;
            } else { attributeValueStart = NULL; attributeValueLength = 0; }

            attribute = (patternAttribute_t*)ngx_array_push(&token->attributes);
            if(attribute == NULL) return 0;

            attribute->name.data = attributeNameStart;
            attribute->name.len = attributeNameLength;
            attribute->value.data = attributeValueStart;
            attribute->value.len = attributeValueLength;
            logHttpDebugMsg2(log, "autols: Attribute information set (%V=\"%V\")", &attribute->name, &attribute->value);
        }

        token->name.data = tokenNameStart;
        token->name.len = tokenNameLength;
        token->startAt = (tokenNameStart - pattern->content.data) - 2;
        token->endAt = (ptn - pattern->content.data) + 1;
        logHttpDebugMsg4(log, "autols: Token information set (%V:%d, %d-%d)", &token->name, tokenNameLength, token->startAt, token->endAt);
    }

    //End Token
    token = (patternToken_t*)ngx_array_push(&pattern->tokens);
    if(token == NULL) return 0;

    token->startAt = token->endAt = pattern->content.len;
    token->name.data = NULL;
    token->name.len = 0;

    logHttpDebugMsg1(log, "autols: Template parsed. %d Tokens found", pattern->tokens.nelts);

    return 1;
}

static pattern_t* getPattern(conConf_t *conConf) {
    pattern_t *pattern, *patternLimit;

    if(conConf->mainConf->patterns.nelts == 0) {
        logHttpDebugMsg0(conConf->log, "autols: Parsing default pattern");
        pattern = (pattern_t*)ngx_array_push(&conConf->mainConf->patterns);
        pattern->content.data = defaultPagePattern;
        pattern->content.len = ngx_strlen(defaultPagePattern);
        pattern->path.data = NULL;
        pattern->path.len = 0;
        if(!parsePattern(pattern, conConf->mainConf->pool, conConf->log)) return NULL;
        return pattern;
    }

    pattern = (pattern_t*)conConf->mainConf->patterns.elts;
    patternLimit = pattern + conConf->mainConf->patterns.nelts;
    while(pattern != patternLimit) {
        if(ngx_str_compare(&conConf->locConf->pagePatternPath, &pattern->path)) return pattern;
        pattern++;
    }

    //TODO: Try find pattern file, parse it, add to list, return it

    logHttpDebugMsg0(conConf->log, "autols: No pattern found");
    return NULL;
}

static int appendTokenValue(patternToken_t *token, strb_t *strb, conConf_t *conConf, fileEntry_t *fileEntry) {
    static strb_t strbA, strbB;
    static ngx_pool_t *pool;

    patternAttribute_t *attribute, *attributeLimit;
    strb_t *curStrb, *prevStrb, *tmpStrb;

    //Cannot use conConf->pool if the instance is used for more than one connection
    if(pool == NULL && (pool = ngx_create_pool(8 * 512, conConf->log)) == NULL) return 0;
    if(!strbA.isInitialized && !strbInit(&strbA, pool, 8 * 256, 8 * 256)) return 0;
    if(!strbB.isInitialized && !strbInit(&strbB, pool, 8 * 256, 8 * 256)) return 0;

    if(token->attributes.nelts == 0) {
        curStrb = strb;
        prevStrb = NULL;

    } else {
        prevStrb = &strbB;
        curStrb = &strbA;
        if(!strbSetSize(&strbA, 0)) return 0;
        if(!strbSetSize(&strbB, 0)) return 0;
    }

    logHttpDebugMsg1(conConf->log, "autols: Appending TokenValue of \"%V\"", &token->name);

    if(fileEntry != NULL) {
        if(ngx_str_compare(&token->name, &tplEntryIsDirectoryStr)) {
            if(!strbAppendSingle(curStrb, fileEntry->isDirectory ? '1' : '0')) return 0;

        } else if(ngx_str_compare(&token->name, &tplEntryModifiedOnStr)) {
            ngx_tm_t tm = fileEntry->modifiedOn;
            if(!strbFormat(curStrb, "%02d-%02d-%d %02d:%02d",
                tm.ngx_tm_mday, tm.ngx_tm_mon, tm.ngx_tm_year,
                tm.ngx_tm_hour, tm.ngx_tm_min)) return 0;

        } else if(ngx_str_compare(&token->name, &tplEntrySizeStr)) {
            if(!strbFormat(curStrb, "%O", fileEntry->size)) return 0;

        } else if(ngx_str_compare(&token->name, &tplEntryNameStr)) {
            if(!strbAppendNgxString(curStrb, &fileEntry->name)) return 0;
            if(fileEntry->isDirectory && !strbAppendSingle(curStrb, '/')) return 0;
        }
    }

    if(ngx_str_compare(&token->name, &tplReplyCharSetStr)) {
        if(!strbAppendNgxString(curStrb, &conConf->locConf->charSet)) return 0;

    } else if(ngx_str_compare(&token->name, &tplRequestUriStr)) {
        if(!strbAppendNgxString(curStrb, &conConf->requestPath)) return 0;
    }

    if(token->attributes.nelts != 0) { curStrb = &strbB; prevStrb = &strbA; }
    attribute = (patternAttribute_t*)token->attributes.elts;
    attributeLimit = attribute + token->attributes.nelts;
    while(attribute != attributeLimit) {
        logHttpDebugMsg1(conConf->log, "autols: Processing attribute \"%V\"", &attribute->name);

        if(ngx_str_compare(&attribute->name, &tplAttStartAtStr)) {
            int32_t toPad = ngx_atoi(attribute->value.data, attribute->value.len);
            toPad = ngx_max(0, toPad - (strb->size - conConf->tplEntryStartPos));

            if(fileEntry == NULL) return 0;
            if(!strbAppendRepeat(curStrb, ' ', toPad)) return 0;
            if(!strbAppendStrb(curStrb, prevStrb)) return 0;

        } else if(ngx_str_compare(&attribute->name, &tplAttNoCountStr)) {
            conConf->tplEntryStartPos += prevStrb->size;

        } else if(ngx_str_compare(&attribute->name, &tplAttMaxLengthStr)) {
            int32_t maxLength = ngx_atoi(attribute->value.data, attribute->value.len);
            if(maxLength < prevStrb->size) {
                strbSetSize(prevStrb, maxLength - 3);
                strbAppendCString(prevStrb, "...");
            }

        } else if(ngx_str_compare(&attribute->name, &tplAttFormatStr)) {
            //Points to the template data so there is always enough space to add a null termination
            u_char oldChar = *(attribute->value.data + attribute->value.len); //TODO: Make less hacky
            *(attribute->value.data + attribute->value.len) = '\0';
            if(!strbTransformStrb(curStrb, prevStrb, strbTransFormat, attribute->value.data)) return 0;
            *(attribute->value.data + attribute->value.len) = oldChar;

        } else if(ngx_str_compare(&attribute->name, &tplAttEscapeStr)) {
            if(ngx_str_compare(&attribute->value, &tplAttUriComponentStr)) {
                if(!strbTransformStrb(curStrb, prevStrb, strbTransEscapeUri, NGX_ESCAPE_URI_COMPONENT)) return 0;
            } else if(ngx_str_compare(&attribute->value, &tplAttUriStr)) {
                if(!strbTransformStrb(curStrb, prevStrb, strbTransEscapeUri, NGX_ESCAPE_URI)) return 0;
            } else if(ngx_str_compare(&attribute->value, &tplAttHttpStr)) {
                if(!strbTransformStrb(curStrb, prevStrb, strbTransEscapeHtml)) return 0;
            } else return 0;
        }
        logHttpDebugMsg1(conConf->log, "autols: Attribute processed", &attribute->name);
        if(curStrb->size != 0) { //Swap StringBuilders iff current one has contents
            tmpStrb = curStrb;
            curStrb = prevStrb;
            prevStrb = tmpStrb;
            strbSetSize(curStrb, 0);
        }
        attribute++;
    }
    if(token->attributes.nelts != 0) strbAppendStrb(strb, prevStrb);

    return 1;
}

static patternToken_t* appendSection(patternToken_t *token, strb_t *strb, u_char *ptn, ngx_str_t *endTokenName, conConf_t *conConf, fileEntriesInfo_T *fileEntriesInfo) {
    patternToken_t *nextToken, *entryStartToken;
    fileEntry_t *fileEntry, *lastFileEntry;

    if(strb == NULL) {
        logHttpDebugMsg0(conConf->log, "autols: Skipping section");
        nextToken = token;
        for(;;) {
            token = nextToken++;
            if(nextToken->name.data == NULL ||
                ngx_str_compare(&nextToken->name, endTokenName)) break;
        }
        if(nextToken->name.data == NULL) return NULL;
        return nextToken;
    }

    //Single non-entries section or beginning of entries section
    logHttpDebugMsg0(conConf->log, "autols: Section Start");
    nextToken = token;
    for(;;) {
        token = nextToken++;

        if(!strbAppendMemory(strb, ptn + token->endAt, nextToken->startAt - token->endAt)) return NULL;

        if(nextToken->name.data == NULL ||
            ngx_str_compare(&nextToken->name, &tplEntryStartStr) ||
            ngx_str_compare(&nextToken->name, endTokenName)) break;

        if(!appendTokenValue(nextToken, strb, conConf, NULL)) return NULL;
    }

    if(nextToken->name.data == NULL) return NULL;
    if(ngx_str_compare(&nextToken->name, endTokenName)) {
        logHttpDebugMsg0(conConf->log, "autols: Section End");
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
        conConf->tplEntryStartPos = strb->size;
        logHttpDebugMsg2(conConf->log, "autols: Processing file %d: %V", fileEntry - (fileEntry_t*)fileEntriesInfo->fileEntries.elts, &fileEntry->name);

        for(;;) {
            token = nextToken++;
            if(!strbAppendMemory(strb, ptn + token->endAt, nextToken->startAt - token->endAt)) return NULL;

            if(nextToken->name.data == NULL || ngx_str_compare(&nextToken->name, &tplEntryEndStr)) break;
            if(!appendTokenValue(nextToken, strb, conConf, fileEntry)) return NULL;
        }
        if(nextToken->name.data == NULL) return NULL;
        fileEntry++;
        if(!strbAppendCString(strb, CRLF)) return NULL;
    }
    strbDecreaseSizeBy(strb, 2);

    //Post FileEntries part of section
    for(;;) {
        token = nextToken++;
        if(!strbAppendMemory(strb, ptn + token->endAt, nextToken->startAt - token->endAt)) return NULL;

        if(nextToken->name.data == NULL || ngx_str_compare(&nextToken->name, endTokenName)) break;
        if(!appendTokenValue(nextToken, strb, conConf, NULL)) return NULL;
    }

    logHttpDebugMsg0(conConf->log, "autols: Section End");

    return nextToken;
}


static ngx_rc_t setRequestPath(conConf_t *conConf) {
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

    logHttpDebugMsg1(conConf->log, "autols: Request Path \"%V\"", reqPath);
    return NGX_OK;
}

static ngx_rc_t openDirectory(conConf_t *conConf, ngx_dir_t *dir) {
    ngx_rc_t   rc;
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
    logHttpDebugMsg0(conConf->log, "autols: Directory opened");
    return NGX_OK;
}

static ngx_rc_t closeDirectory(conConf_t *conConf, ngx_dir_t *dir, int throwError) {
    if(ngx_close_dir(dir) == NGX_ERROR) {
        ngx_log_error(
            NGX_LOG_ALERT, conConf->log, ngx_errno,
            ngx_close_dir_n " \"%V\" failed", conConf->requestPath);
    }

    logHttpDebugMsg1(conConf->log, "autols: Directory closed (WithErrors=%d)", throwError);
    return !throwError ? NGX_OK : (conConf->request->header_sent ? NGX_ERROR : NGX_HTTP_INTERNAL_SERVER_ERROR);
}

static ngx_rc_t readDirectory(conConf_t *conConf, ngx_dir_t *dir) {
    ngx_err_t err;

    ngx_set_errno(0);
    if(ngx_read_dir(dir) == NGX_ERROR) {
        err = ngx_errno;

        if(err != NGX_ENOMOREFILES) {
            ngx_log_error(
                NGX_LOG_CRIT, conConf->log, err,
                ngx_read_dir_n " \"%V\" failed", conConf->requestPath.data);

            return closeDirectory(conConf, dir, CLOSE_DIRECTORY_ERROR);
        }
        logHttpDebugMsg0(conConf->log, "autols: No more files");
        return NGX_DONE;
    }
    return NGX_AGAIN;
}

static ngx_rc_t checkFSEntry(conConf_t *conConf, ngx_dir_t *dir, size_t fileNameLength) {
    ngx_str_t *reqPath;
    size_t reqPathCap;
    ngx_err_t err;
    u_char *last;

    if(dir->valid_info) return NGX_OK;

    reqPath = &conConf->requestPath;
    reqPathCap = conConf->requestPathCapacity;
    last = reqPath->data + reqPath->len;

    //Building file path: include terminating '\0'
    logHttpDebugMsg0(conConf->log, "autols: Checking if filepath is valid");
    if(reqPath->len + fileNameLength + 1 > reqPathCap) {
        u_char *filePath = reqPath->data;

        logHttpDebugMsg0(conConf->log, "autols: Increasing path capacity to store full filepath");
        reqPathCap = reqPath->len + fileNameLength + 1 + STRING_PREALLOCATE;

        filePath = (u_char*)ngx_pnalloc(conConf->pool, reqPathCap);
        if(filePath == NULL) return closeDirectory(conConf, dir, CLOSE_DIRECTORY_ERROR);

        last = ngx_cpystrn(filePath, reqPath->data, reqPath->len + 1);

        reqPath->data = filePath;
        conConf->requestPathCapacity = reqPathCap;
    }
    last = ngx_cpystrn(last, ngx_de_name(dir), fileNameLength + 1);

    logHttpDebugMsg1(conConf->log, "autols: Full path: \"%s\"", reqPath->data);
    if(ngx_de_info(reqPath->data, dir) == NGX_FILE_ERROR) {
        err = ngx_errno;

        if(err != NGX_ENOENT && err != NGX_ELOOP) {
            ngx_log_error(NGX_LOG_CRIT, conConf->log, err,
                "autols: " ngx_de_info_n " \"%V\" failed", reqPath);

            if(err == NGX_EACCES) return NGX_EACCES; //TODO: Make sure there are no flag collisions
            return closeDirectory(conConf, dir, CLOSE_DIRECTORY_ERROR);
        }

        if(ngx_de_link_info(reqPath->data, dir) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_CRIT, conConf->log, ngx_errno,
                "autols: " ngx_de_link_info_n " \"%V\" failed", reqPath);

            return closeDirectory(conConf, dir, CLOSE_DIRECTORY_ERROR);
        }
    }

    return NGX_OK;
}

static ngx_rc_t sendHeaders(conConf_t *conConf, ngx_dir_t *dir) {
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

    logHttpDebugMsg0(conConf->log, "autols: Header sent");
    return NGX_OK;
}

static ngx_rc_t getFiles(conConf_t *conConf, ngx_dir_t *dir, fileEntriesInfo_T *fileEntriesInfo) {
    ngx_int_t    gmtOffset;
    ngx_str_t   *reqPath;
    fileEntry_t *entry;
    ngx_rc_t rc;

    gmtOffset = (ngx_timeofday())->gmtoff * 60;
    reqPath   = &conConf->requestPath;

    fileEntriesInfo->totalFileNamesLength =
        fileEntriesInfo->totalFileNamesLengthUriEscaped =
        fileEntriesInfo->totalFileNamesLengthHtmlEscaped = 0;

    if(conConf->requestPathCapacity <= reqPath->len + 1) return NGX_ERROR;
    reqPath->data[reqPath->len++] = '/';
    reqPath->data[reqPath->len] = '\0';

    counters[CounterFileCount] = 0;
    logHttpDebugMsg0(conConf->log, "autols: ##Iterating files");
    while((rc = readDirectory(conConf, dir)) == NGX_AGAIN) {
        size_t fileNameLength;

        counters[CounterFileCount]++;
        fileNameLength = ngx_de_namelen(dir);
        fileEntriesInfo->totalFileNamesLength += fileNameLength;
        logHttpDebugMsg1(conConf->log, "autols: #File \"%s\"", ngx_de_name(dir));

        //Skip "current directory" path
        if(ngx_de_name(dir)[0] == '.' && ngx_de_name(dir)[1] == '\0') continue;

        rc = checkFSEntry(conConf, dir, fileNameLength);
        if(rc == NGX_EACCES) continue;
        if(rc != NGX_OK) return rc;

        //Filter files specified in config
        if(filterFile(NULL, conConf->locConf->entryIgnores, conConf->log)) continue;

        logHttpDebugMsg1(conConf->log, "autols: Retrieving file info (Index=%d)", fileEntriesInfo->fileEntries.nelts);
        entry = (fileEntry_t*)ngx_array_push(&fileEntriesInfo->fileEntries);
        if(entry == NULL) return closeDirectory(conConf, dir, CLOSE_DIRECTORY_ERROR);

        entry->name.len = fileNameLength;
        entry->name.data = (u_char*)ngx_pnalloc(conConf->pool, fileNameLength + 1);
        if(entry->name.data == NULL) return closeDirectory(conConf, dir, CLOSE_DIRECTORY_ERROR);
        ngx_cpystrn(entry->name.data, ngx_de_name(dir), fileNameLength + 1);

        //With local time offset if specified
        ngx_gmtime(ngx_de_mtime(dir) + gmtOffset * conConf->locConf->localTime, &entry->modifiedOn);
        entry->isDirectory = ngx_de_is_dir(dir);
        entry->size = ngx_de_size(dir);

        fileEntriesInfo->totalFileNamesLengthUriEscaped += entry->nameLenAsUri = fileNameLength +
            ngx_escape_uri(NULL, entry->name.data, entry->name.len, NGX_ESCAPE_URI_COMPONENT);

        fileEntriesInfo->totalFileNamesLengthHtmlEscaped += entry->nameLenAsHtml = fileNameLength +
            ngx_escape_html(NULL, entry->name.data, entry->name.len);
        logHttpDebugMsg0(conConf->log, "autols: File info retrieved");

    }
    if(rc != NGX_DONE) return rc;
    logHttpDebugMsg0(conConf->log, "autols: Done iterating files");

    return NGX_OK;
}

static ngx_rc_t createReplyBody(conConf_t *conConf, fileEntriesInfo_T *fileEntriesInfo, ngx_chain_t *out) {
    patternToken_t *token, *nextToken;
    ngx_array_t *tokens;
    pattern_t *pattern;
    size_t bufSize;
    int doAppend;
    strb_t strb;
    u_char *tpl;

    pattern = getPattern(conConf);
    if(pattern == NULL) return NGX_ERROR;

    tpl = pattern->content.data;

    bufSize =
        pattern->content.len +
        (
        conConf->locConf->createBody +
        conConf->locConf->createJsVariable
        ) * 4 * 256 * fileEntriesInfo->fileEntries.nelts;

    logHttpDebugMsg1(conConf->log, "autols: Estimated buffer size: %d", bufSize);

    //if(!strbInit(&strb, conConf->pool, bufSize, bufSize)) return NGX_ERROR;
    if(!strbInit(&strb, conConf->request->pool, bufSize, bufSize)) return NGX_ERROR;
    appendConfig(&strb, conConf);

    tokens = &pattern->tokens;
    token = (patternToken_t*)tokens->elts;
    nextToken = token;

    logHttpDebugMsg0(conConf->log, "autols: ##Applying template");
    for(;;) {
        token = nextToken++;
        logHttpDebugMsg1(conConf->log, "autols: #Processing Token: %V", &nextToken->name);
        if(!strbAppendMemory(&strb, tpl + token->endAt, nextToken->startAt - token->endAt)) return NGX_ERROR;

        if(ngx_str_compare(&nextToken->name, &tplJsVariableStartStr)) {
            doAppend = conConf->locConf->createJsVariable;
            nextToken = appendSection(nextToken, (doAppend ? &strb : NULL), tpl, &tplJsVariableEndStr, conConf, fileEntriesInfo);

        } else if(ngx_str_compare(&nextToken->name, &tplJsSourceStartStr)) {
            doAppend = conConf->locConf->jsSourcePath.len != 0;
            nextToken = appendSection(nextToken, (doAppend ? &strb : NULL), tpl, &tplJsSourceEndStr, conConf, fileEntriesInfo);

        } else if(ngx_str_compare(&nextToken->name, &tplCssSourceStartStr)) {
            doAppend = conConf->locConf->cssSourcePath.len != 0;
            nextToken = appendSection(nextToken, (doAppend ? &strb : NULL), tpl, &tplCssSourceEndStr, conConf, fileEntriesInfo);

        } else if(ngx_str_compare(&nextToken->name, &tplBodyStartStr)) {
            doAppend = conConf->locConf->createBody;
            nextToken = appendSection(nextToken, (doAppend ? &strb : NULL), tpl, &tplBodyEndStr, conConf, fileEntriesInfo);

        } else if(nextToken->name.data != NULL) {
            if(!appendTokenValue(nextToken, &strb, conConf, NULL)) return NGX_ERROR;
        }

        if(nextToken == NULL) return NGX_ERROR;
        if(nextToken->name.data == NULL) break;
    }
    logHttpDebugMsg0(conConf->log, "autols: Reply body generated");

    if(conConf->request == conConf->request->main) {
        logHttpDebugMsg0(conConf->log, "autols: strb.lastLink->buf->last_buf = 1");
        strb.lastLink->buf->last_buf = 1;
    }

    logHttpDebugMsg1(conConf->log, "autols: strb.lastLink->buf->last_in_chain = %D", strb.lastLink->buf->last_in_chain);
    logHttpDebugMsg1(conConf->log, "autols: strb.startLink->next = %d", strb.startLink->next);

    //appendConfig(&strb, conConf);

    *out = *strb.startLink;
    return NGX_OK;
}


static ngx_rc_t ngx_http_autols_handler(ngx_http_request_t *r) {
    fileEntriesInfo_T fileEntriesInfo;
    conConf_t  conConf;
    ngx_chain_t       out;
    ngx_dir_t         dir;
    ngx_rc_t          rc;

    counters[CounterHandlerInvoke]++;
    connLog = r->connection->log;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "autols: Invoked");
    if(r->uri.data[r->uri.len - 1] != '/') return NGX_DECLINED;
    if(!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) return NGX_DECLINED;

    //Get Settings
    conConf.mainConf = (ngx_http_autols_main_conf_t*)ngx_http_get_module_main_conf(r, ngx_http_autols_module);
    conConf.locConf = (ngx_http_autols_loc_conf_t*)ngx_http_get_module_loc_conf(r, ngx_http_autols_module);
    conConf.log = r->connection->log;
    conConf.request = r;

    //Check if we're enabled
    if(!conConf.locConf->enable) {
        logHttpDebugMsg0(conConf.log, "autols: Declined request");
        return NGX_DECLINED;
    }
    logHttpDebugMsg0(conConf.log, "autols: Accepted request");

    //Get Request Path
    rc = setRequestPath(&conConf);
    if(rc != NGX_OK) return rc;

    //Open Directory
    rc = openDirectory(&conConf, &dir);
    if(rc != NGX_OK) return rc;

    //Send Header
    rc = sendHeaders(&conConf, &dir);
    if(rc != NGX_OK) return rc;

    //Create local pool and initialize file entries array
    conConf.pool = ngx_create_pool((sizeof(fileEntry_t) + 20) * 400, conConf.log); //TODO: Constant

    if(ngx_array_init(&fileEntriesInfo.fileEntries, conConf.pool, 40, sizeof(fileEntry_t)) != NGX_OK) {
        return closeDirectory(&conConf, &dir, CLOSE_DIRECTORY_ERROR);
    }

    //Get File Entries
    rc = getFiles(&conConf, &dir, &fileEntriesInfo);
    if(rc != NGX_OK) return rc;

    //Close Directory
    rc = closeDirectory(&conConf, &dir, CLOSE_DIRECTORY_OK);
    if(rc != NGX_OK) return rc;

    //Sort File Entries
    if(fileEntriesInfo.fileEntries.nelts > 1) {
        logHttpDebugMsg1(conConf.log, "autols: Sorting %d file entries", fileEntriesInfo.fileEntries.nelts);
        ngx_qsort(fileEntriesInfo.fileEntries.elts,
            (size_t)fileEntriesInfo.fileEntries.nelts,
            sizeof(fileEntry_t), fileEntryComparer);
    }

    //Create Reply Body
    out.buf = NULL; out.next = NULL;
    rc = createReplyBody(&conConf, &fileEntriesInfo, &out);
    if(rc != NGX_OK) return rc;

    rc = ngx_http_output_filter(r, &out);
    ngx_destroy_pool(conConf.pool);

    return rc;
}

static void* ngx_http_autols_create_main_conf(ngx_conf_t *cf) {
    ngx_http_autols_main_conf_t *conf;
    conf = (ngx_http_autols_main_conf_t*)ngx_pcalloc(cf->pool, sizeof(ngx_http_autols_main_conf_t));
    if(conf == NULL) return NULL;

    counters[CounterMainCreateCall]++;
    return conf;
}
static char* ngx_http_autols_init_main_conf(ngx_conf_t *cf, void *conf) {
    ngx_http_autols_main_conf_t *mainConf = (ngx_http_autols_main_conf_t*)conf;
    mainConf->pool = ngx_create_pool(1024 * 1024, cf->log); //TODO: Shared memory

    ngx_array_init(&mainConf->patterns, mainConf->pool, 2, sizeof(pattern_t));

    counters[CounterMainMergeCall]++; 
    return NGX_CONF_OK;
}

static void* ngx_http_autols_create_loc_conf(ngx_conf_t *cf) {
    ngx_http_autols_loc_conf_t  *conf;
    conf = (ngx_http_autols_loc_conf_t*)ngx_pcalloc(cf->pool, sizeof(ngx_http_autols_loc_conf_t));
    if(conf == NULL) return NULL;

    conf->enable = NGX_CONF_UNSET;
    conf->createJsVariable = NGX_CONF_UNSET;
    conf->createBody = NGX_CONF_UNSET;
    conf->localTime = NGX_CONF_UNSET;
    conf->entryIgnores = (ngx_array_t*)NGX_CONF_UNSET_PTR;

    counters[CounterLocCreateCall]++;
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
    ngx_conf_merge_str_value(conf->pagePatternPath, prev->pagePatternPath, "");
    ngx_conf_merge_ptr_value(conf->entryIgnores, prev->entryIgnores, NULL);

    counters[CounterLocMergeCall]++;
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


static char* ngx_conf_autols_regex_array_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    u_char regexCompileErrorMsg[NGX_MAX_CONF_ERRSTR];
    ngx_str_t *regexPattern, *regexPatternLast;
    ngx_regex_compile_t regexCompile;
    ngx_regex_elt_t *regexEntry;
    ngx_array_t **dstArray;

    dstArray = (ngx_array_t **) ((char*)conf + cmd->offset);
    if(*dstArray == NGX_CONF_UNSET_PTR) {
        *dstArray = ngx_array_create(cf->pool, 4, sizeof(ngx_regex_t));
        if(*dstArray == NULL) {
            return (char*)NGX_CONF_ERROR;
        }
    } else return "is duplicate";

    ngx_memzero(&regexCompile, sizeof(ngx_regex_compile_t));
    regexCompile.err.data = regexCompileErrorMsg;
    regexCompile.err.len  = NGX_MAX_CONF_ERRSTR;
    regexCompile.pool     = cf->pool;

    regexPattern = (ngx_str_t*)cf->args->elts;
    regexPatternLast = regexPattern + cf->args->nelts;
    for(;regexPattern != regexPatternLast; regexPattern++) {
        regexCompile.pattern = *regexPattern;
        if(ngx_regex_compile(&regexCompile) != NGX_OK) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "%V", &regexCompile.err);
            return (char*)NGX_CONF_ERROR;
        }

        regexEntry = (ngx_regex_elt_t*)ngx_array_push(*dstArray);
        if(regexEntry == NULL) return (char*)NGX_CONF_ERROR;
        regexEntry->regex = regexCompile.regex;
        regexEntry->name = regexPattern->data;
    }

    return NGX_CONF_OK;
}

static char* ngx_conf_autols_regex_then_string_array_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
#if NGX_PCRE
    return ngx_conf_autols_regex_array_slot(cf, cmd, conf);
#else
    return ngx_conf_set_str_array_slot(cf, cmd, conf);
#endif
}