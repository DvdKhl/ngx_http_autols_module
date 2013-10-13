#include "ngx_http_autols_module.h"

static ngx_log_t *alsLog;
static ngx_tm_t *processHandlerFirstInvokeOn;

static const char *counterNames[] = {
	"MainMergeCall", "SrvMergeCall", "LocMergeCall",
	"MainCreateCall", "SrvCreateCall", "LocCreateCall",
	"HandlerInvoke", "TemplateParse", "FileCount"
};
static int counters[CounterLimit];

static char defaultPagePattern[] =
"<!DOCTYPE html>" CRLF
"<html>" CRLF
"  <head>" CRLF
"    <meta charset=\"<!--[ReplyCharSet]-->\">" CRLF
"    <title><!--[RequestUri]--> (AutoLS)</title><!--{JSVariable}-->" CRLF
"    <script type=\"text/javascript\">" CRLF
"      var dirListing = [<!--{EntryLoop}-->" CRLF
"        {" CRLF
"          \"isDirectory\": <!--[EntryIsDirectory]-->," CRLF
"          \"modifiedOn\": \"<!--[EntryModifiedOn]-->\"," CRLF
"          \"size\": <!--[EntrySize]-->," CRLF
"          \"name\": \"<!--[EntryName]-->\"" CRLF
"        },<!--{/EntryLoop}-->" CRLF
"      ];" CRLF
"    </script><!--{/JSVariable}--><!--{JSSource}-->" CRLF
"    <script type=\"text/javascript\" src=\"<!--[JSSource]-->\"></script><!--{/JSSource}--><!--{CSSSource}-->" CRLF
"    <link rel=\"stylesheet\" type=\"text/css\" href=\"<!--[CSSSource]-->\"><!--{/CSSSource}-->" CRLF
"    <!--[Head]-->" CRLF
"  </head>" CRLF
"  <body bgcolor=\"white\"><!--{Body}-->" CRLF
"    <!--[Header]-->" CRLF
"    <h1>Index of <!--[RequestUri]--></h1>" CRLF
"    <hr>" CRLF
"    <pre><!--{EntryLoop}--><a href=\"<!--[EntryName?Escape=Uri&NoCount]-->\"><!--[EntryName?MaxLength=66&Utf8Count]--></a><!--[EntryModifiedOn?StartAt=82]--> <!--[EntrySize?Format=%24s]-->\r\n<!--{/EntryLoop}--></pre><!--{/Body}-->" CRLF
"    <!--[Footer]-->" CRLF
"  </body>" CRLF
"</html>";

static void appendConfigPattern(stringBuilder *strb, alsPattern *pattern, ngx_array_t *tokens, int depth) {
	alsPatternToken *token = (alsPatternToken*)tokens->elts;
	alsPatternToken *tokenLimit = token + tokens->nelts;
	while(token != tokenLimit) {
		strbAppendRepeat(strb, ' ', depth);
		strbNgxFormat(strb, "(N=%V, S=%d, E=%d, AC=%d)" CRLF, &token->name, token->start - (char*)pattern->content.data, token->end - (char*)pattern->content.data, token->attributes.nelts);
		appendConfigPattern(strb, pattern, &token->children, depth + 1);
		token++;
	}
	pattern++;
}
static void appendConfig(stringBuilder *strb, alsConnectionConfig *conConf) {
	char cwd[NGX_MAX_PATH];
	int i;

	if(!ngx_getcwd(cwd, NGX_MAX_PATH)) logHttpDebugMsg0(alsLog, "autols: Getting cwd failed");
	
	//ngx_file_t file; 
	//ngx_memzero(&file, sizeof(ngx_file_t));
	//
	//file.name.data = (u_char*)"README";
	//file.name.len = 6;
	//file.log = alsLog;
	//
	//file.fd = ngx_open_file((u_char*)"README", NGX_FILE_RDONLY, NGX_FILE_OPEN, NGX_FILE_DEFAULT_ACCESS);
	//if(file.fd == NGX_INVALID_FILE) logHttpDebugMsg0(alsLog, "autols: Invalid File");
	//	
	//u_char buf[1024];
	//ngx_memzero(buf, 1024);
	//logHttpDebugMsg0(alsLog, "autols: 1");
	//ngx_read_file(&file, buf, 1024, 0);
	//strbFormat(strb, "README = %s" CRLF, buf);
	//logHttpDebugMsg0(alsLog, "autols: 2");
	//
	//
	//if(ngx_close_file(file.fd) == NGX_FILE_ERROR) return;


	logHttpDebugMsg0(alsLog, "autols: Printing Globals");
	strbAppendCString(strb, "<pre>#Global" CRLF);
	strbFormat(strb, "ngx_process = %d" CRLF, ngx_process);
	ngx_tm_t tm = *processHandlerFirstInvokeOn;
	strbFormat(strb, "processHandlerFirstInvokeOn = %02d-%02d-%d %02d:%02d" CRLF, tm.ngx_tm_mday, tm.ngx_tm_mon, tm.ngx_tm_year, tm.ngx_tm_hour, tm.ngx_tm_min);
	strbFormat(strb, "Current Working Directory = %s" CRLF, cwd);

	logHttpDebugMsg0(alsLog, "autols: Printing Main Config");
	strbAppendCString(strb, CRLF "#Main Config" CRLF);
	tm = conConf->mainConf->createdOn;
	strbFormat(strb, "mainConf->createdOn = %02d-%02d-%d %02d:%02d" CRLF, tm.ngx_tm_mday, tm.ngx_tm_mon, tm.ngx_tm_year, tm.ngx_tm_hour, tm.ngx_tm_min);

	logHttpDebugMsg0(alsLog, "autols: Printing Main Config Patterns");
	strbAppendCString(strb, "Parsed Patterns:" CRLF);
	alsPattern *pattern, *patternLimit;
	pattern = (alsPattern*)conConf->mainConf->patterns.elts;
	patternLimit = pattern + conConf->mainConf->patterns.nelts;
	while(pattern != patternLimit) {
		strbNgxFormat(strb, "Path=\"%V\"" CRLF, &pattern->path);
		appendConfigPattern(strb, pattern, &pattern->tokens, 0);
		strbAppendCString(strb, CRLF);
		pattern++;
	}

	logHttpDebugMsg0(alsLog, "autols: Printing Location Config");
	strbAppendCString(strb, "#Location Config" CRLF);
	tm = conConf->locConf->createdOn;
	strbFormat(strb, "locConf->createdOn = %02d-%02d-%d %02d:%02d" CRLF, tm.ngx_tm_mday, tm.ngx_tm_mon, tm.ngx_tm_year, tm.ngx_tm_hour, tm.ngx_tm_min);
	strbFormat(strb, "locConf->enable = %d" CRLF, conConf->locConf->enable);
	strbFormat(strb, "locConf->localTime = %d" CRLF, conConf->locConf->localTime);
	strbFormat(strb, "locConf->patternPath = %d" CRLF, conConf->locConf->patternPath);
	strbAppendCString(strb, "locConf->entryIgnores = {");
	if(conConf->locConf->entryIgnores && conConf->locConf->entryIgnores->nelts) {

#if USE_REGEX
		strbAppendCString(strb, (char*)((ngx_regex_elt_t*)conConf->locConf->entryIgnores->elts)->name);
		for(i = 1; i < (int)conConf->locConf->entryIgnores->nelts; i++) {
			strbAppendCString(strb, ", ");
			strbAppendCString(strb, (char*)((ngx_regex_elt_t*)conConf->locConf->entryIgnores->elts + i)->name);
		}
#else
		strbAppendNgxString(strb, (ngx_str_t*)conConf->locConf->entryIgnores->elts);
		for(i = 1; i < (int)conConf->locConf->entryIgnores->nelts; i++) {
			strbAppendCString(strb, ", ");
			strbAppendNgxString(strb, (ngx_str_t*)conConf->locConf->entryIgnores->elts + i);
		}
#endif

	}

	strbAppendCString(strb, "}" CRLF);

	strbAppendCString(strb, "locConf->sections = {");
	if(conConf->locConf->sections && conConf->locConf->sections->nelts) {
		strbAppendNgxString(strb, (ngx_str_t*)conConf->locConf->sections->elts);
		for(i = 1; i < (int)conConf->locConf->sections->nelts; i++) {
			strbAppendCString(strb, ", ");
			strbAppendNgxString(strb, (ngx_str_t*)conConf->locConf->sections->elts + i);
		}
	}
	strbAppendCString(strb, "}" CRLF);

	strbAppendCString(strb, "locConf->keyValuePairs = {");
	if(conConf->locConf->keyValuePairs && conConf->locConf->keyValuePairs->nelts) {
		ngx_keyval_t *pair = (ngx_keyval_t*)conConf->locConf->keyValuePairs->elts;
		strbNgxFormat(strb, "(%V = %V)", &pair->key, &pair->value);
		for(i = 1; i < (int)conConf->locConf->keyValuePairs->nelts; i++) {
			pair = (ngx_keyval_t*)conConf->locConf->keyValuePairs->elts + i;
			strbNgxFormat(strb, ", (%V = %V)", &pair->key, &pair->value);
		}
	}
	strbAppendCString(strb, "}" CRLF);

	logHttpDebugMsg0(alsLog, "autols: Printing Connection Config");
	strbAppendCString(strb, CRLF "#Connection Config" CRLF);
	strbNgxFormat(strb, "requestPath = %V" CRLF, &conConf->requestPath);
	strbFormat(strb, "requestPathCapacity = %d" CRLF, conConf->requestPathCapacity);
	strbFormat(strb, "ptnEntryStartPos = %d" CRLF, conConf->ptnEntryStartPos);
	strbNgxFormat(strb, "request->args = %V" CRLF, &conConf->request->args);
	strbNgxFormat(strb, "request->headers_in.user_agent = %V" CRLF, &conConf->request->headers_in.user_agent->value);

	logHttpDebugMsg0(alsLog, "autols: Printing Counters");
	strbAppendCString(strb, CRLF "#Counters" CRLF);
	for(i = 0; i < CounterLimit; i++) strbFormat(strb, "%s = %d" CRLF, counterNames[i], counters[i]);

	strbAppendCString(strb, "</pre>" CRLF CRLF);
}

static int ngx_libc_cdecl fileEntryComparer(const void *one, const void *two) {
	alsFileEntry *first = (alsFileEntry*)one;
	alsFileEntry *second = (alsFileEntry*)two;

	if(first->isDirectory && !second->isDirectory) return -1; //move the directories to the start
	if(!first->isDirectory && second->isDirectory) return 1; //move the directories to the start

	return (int)ngx_strcmp(first->name.data, second->name.data);
}
static int ngx_libc_cdecl stringLenComparer(u_char *one, size_t oneLen, u_char *two, size_t twoLen) {
	size_t n = ngx_min(oneLen, twoLen);
	while(n-- != 0) if(one[n] != one[n]) return one[n] - one[n];
	return oneLen - twoLen;
}
static int ngx_libc_cdecl ngxStringComparer(const void *one, const void *two) {
	ngx_str_t *first = (ngx_str_t*)one;
	ngx_str_t *second = (ngx_str_t*)two;
	return stringLenComparer(first->data, first->len, second->data, second->len);
}
static int ngx_libc_cdecl ngxKeyValElemComparer(const void *one, const void *two) {
	return ngxStringComparer(&((ngx_keyval_t*)one)->key, &((ngx_keyval_t*)two)->key);
}
static int ngx_libc_cdecl ngxKeyValKeyComparer(const void *one, const void *two) {
	return ngxStringComparer((ngx_str_t*)one, &((ngx_keyval_t*)two)->key);
}

static int appendFileName(alsConnectionConfig *conConf, ngx_dir_t *dir, ngx_str_t *dst, size_t *dstCap) {
	size_t fileNameLength;
	u_char *last;

	fileNameLength = ngx_de_namelen(dir);
	last = dst->data + dst->len;

	//Building file path: include terminating '\0'
	logHttpDebugMsg0(alsLog, "autols: Checking if filepath is valid");
	if(dst->len + fileNameLength + 1 > *dstCap) {
		u_char *filePath = dst->data;

		logHttpDebugMsg0(alsLog, "autols: Increasing path capacity to store full filepath");
		*dstCap = dst->len + fileNameLength + 1 + STRING_PREALLOCATE;

		filePath = (u_char*)ngx_pnalloc(conConf->request->pool, *dstCap);
		if(filePath == NULL) return 0;

		last = ngx_cpystrn(filePath, dst->data, dst->len + 1);

		dst->data = filePath;
	}
	last = ngx_cpystrn(last, ngx_de_name(dir), fileNameLength + 1);
	dst->len += fileNameLength;

	return 1;
}

#if !USE_REGEX
static int filterFileExactMatch(ngx_str_t *path, ngx_array_t *filters, ngx_log_t *log) {
	ngx_str_t *filter, *filterLast;
	int hasMatch = 0;

	if(filters == NULL) return 0;

	filter = (ngx_str_t*)filters->elts;
	filterLast = filter + filters->nelts;
	for(; filter != filterLast; filter++) {
		if(ngx_str_compare(filter, path)) {
			hasMatch = 1;
			break;
		}
	}

	return hasMatch;
}
#endif

static int filterFile(ngx_str_t *path, ngx_array_t *filters, ngx_log_t *log) {
	if(filters == NULL) return 0;

#if USE_REGEX
	return ngx_regex_exec_array(filters, path, log) == NGX_OK;
#else
	return filterFileExactMatch(path, filters, log);
#endif
}

static int parsePatternSub(ngx_array_t *tokens, ngx_str_t *content, u_char **current, ngx_pool_t *pool) {
	u_char *cur = *current;
	u_char *curLimit = cur + content->len;

	while(cur < curLimit - 9) { //<!--{A}-->
		if(cur[0] != '<' || cur[1] != '!' || cur[2] != '-' || cur[3] != '-' || (cur[4] != '{' && cur[4] != '[')) {
			cur++;
			continue;
		}
		cur += 4;

		logHttpDebugMsg1(alsLog, "autols: #Start of tag found at %d", *current - content->data);

		int isSection = *cur++ == '{';
		u_char closingChar = isSection ? '}' : ']';

		int isSectionEnd = *cur == '/';
		if(isSectionEnd) cur++;

		u_char *tokenNameStart = cur;
		while(((*cur >= 'A' && *cur <= 'Z') || (*cur >= 'a' && *cur <= 'z') || (*cur >= '0' && *cur <= '9')) && ++cur < curLimit);
		int tokenNameLength = cur - tokenNameStart;

		if(tokenNameLength <= 0) return 0;
		if(cur >= curLimit) return 0;
		if(*cur != closingChar && (*cur != '?' || isSectionEnd)) return 0;
		logHttpDebugMsg1(alsLog, "autols: Token length is %d", tokenNameLength);

		alsPatternToken *token = (alsPatternToken*)ngx_array_push(tokens);
		if(token == NULL) return 0;

		if(ngx_array_init(&token->attributes, pool, 2, sizeof(alsPatternAttribute)) != NGX_OK) return 0;

		while(*cur == '?' || *cur == '&') {
			logHttpDebugMsg1(alsLog, "autols: Start of attribute found at %d", cur - content->data);

			u_char *attributeNameStart = ++cur;
			while(((*cur >= 'A' && *cur <= 'Z') || (*cur >= 'a' && *cur <= 'z') || (*cur >= '0' && *cur <= '9')) && ++cur < curLimit);
			int attributeNameLength = cur - attributeNameStart;
			if(attributeNameLength == 0 || cur >= curLimit || (*cur != '=' && *cur != closingChar && *cur != '&')) return 0;

			u_char *attributeValueStart;
			int attributeValueLength;
			if(*cur == '=') {
				attributeValueStart = ++cur;
				while((*cur != closingChar && *cur != '&') && ++cur < curLimit);
				attributeValueLength = cur - attributeValueStart;
				if(attributeValueLength == 0 || cur >= curLimit) return 0;
			} else {
				attributeValueStart = NULL; attributeValueLength = 0;
			}

			alsPatternAttribute *attribute = (alsPatternAttribute*)ngx_array_push(&token->attributes);
			if(attribute == NULL) return 0;

			attribute->name.data = attributeNameStart;
			attribute->name.len = attributeNameLength;
			attribute->value.data = attributeValueStart;
			attribute->value.len = attributeValueLength;
			logHttpDebugMsg2(alsLog, "autols: Attribute information set (%V=\"%V\")", &attribute->name, &attribute->value);

			//TODO if attribute->value is surrounded by "[]" get value from config (autols_variable=(name=value))
		}

		if(cur[1] != '-' || cur[2] != '-' || cur[3] != '>') return 0;
		cur += 4;

		if(ngx_array_init(&token->children, pool, 8, sizeof(alsPatternToken)) != NGX_OK) return 0;

		token->name.data = tokenNameStart;
		token->name.len = tokenNameLength;
		token->start = (char*)tokenNameStart - (isSectionEnd ? 6 : 5);

		logHttpDebugMsg4(alsLog, "autols: Token information set (%V:%d, %d-%d)", &token->name, tokenNameLength, token->start, token->end);

		if(isSection && !isSectionEnd) {
			alsPatternToken *childStartToken = (alsPatternToken*)ngx_array_push(&token->children);
			if(ngx_array_init(&childStartToken->children, pool, 1, sizeof(alsPatternToken)) != NGX_OK) return 0;
			if(ngx_array_init(&childStartToken->attributes, pool, 1, sizeof(alsPatternAttribute)) != NGX_OK) return 0;
			childStartToken->start = token->start;
			childStartToken->end = (char*)cur;
			childStartToken->name = token->name;

			if(!parsePatternSub(&token->children, content, &cur, pool)) return 0;
		}
		token->end = (char*)cur;

		if(isSectionEnd) break;
	}

	*current = cur;
	return 1;
}
static int parsePattern(ngx_array_t *tokens, ngx_str_t *content, ngx_pool_t *pool) {
	//Start Token
	alsPatternToken *token = (alsPatternToken*)ngx_array_push(tokens);
	if(token == NULL) return 0;

	if(ngx_array_init(&token->children, pool, 1, sizeof(alsPatternToken)) != NGX_OK) return 0;
	if(ngx_array_init(&token->attributes, pool, 1, sizeof(alsPatternAttribute)) != NGX_OK) return 0;
	token->start = token->end = (char*)content->data;
	token->name.data = NULL;
	token->name.len = 0;

	u_char *current = content->data;
	parsePatternSub(tokens, content, &current, pool);

	//End Token
	token = (alsPatternToken*)ngx_array_push(tokens);
	if(token == NULL) return 0;

	if(ngx_array_init(&token->children, pool, 1, sizeof(alsPatternToken)) != NGX_OK) return 0;
	if(ngx_array_init(&token->attributes, pool, 1, sizeof(alsPatternAttribute)) != NGX_OK) return 0;
	token->start = token->end = (char*)content->data + content->len;
	token->name.data = NULL;
	token->name.len = 0;

	logHttpDebugMsg0(alsLog, "autols: Template parsed");

	return 1;
}

static alsPattern* getPattern(ngx_str_t *patternPath, ngx_array_t *patterns) {
	alsPattern *pattern = (alsPattern*)patterns->elts;
	alsPattern *patternLimit = pattern + patterns->nelts;
	while(pattern != patternLimit) {
		if(ngx_str_compare(patternPath, &pattern->path)) return pattern;
		pattern++;
	}

	logHttpDebugMsg0(alsLog, "autols: No pattern found");
	return NULL;
}

static int applyPatternProcessAttributes(stringBuilder *strbValue, ngx_array_t *attributes, alsConnectionConfig *conConf, int32_t strbSize) {
	alsPatternAttribute *attribute = (alsPatternAttribute*)attributes->elts;
	alsPatternAttribute *attributeLimit = attribute + attributes->nelts;
	while(attribute != attributeLimit) {
		//logHttpDebugMsg1(alsLog, "autols: Processing attribute \"%V\"", &attribute->name);

		if(ngx_cstr_compare(&attribute->name, "StartAt")) {
			int32_t toPad = ngx_atoi(attribute->value.data, attribute->value.len);
			toPad = ngx_max(0, toPad - (strbSize - conConf->ptnEntryStartPos));
			if(!strbTransform(strbValue, strbTransPadLeft, ' ', toPad + strbValue->size)) return 0;

		} else if(ngx_cstr_compare(&attribute->name, "AsLossyAscii")) {
			if(attribute->value.len && !strbTransform(strbValue, strbTransAsLossyAscii, attribute->value.data)) return 0;

		} else if(ngx_cstr_compare(&attribute->name, "Utf8Count")) {
			conConf->ptnEntryStartPos += strbValue->size - strbNgxUtf8Length(strbValue);

		} else if(ngx_cstr_compare(&attribute->name, "NoCount")) {
			conConf->ptnEntryStartPos += strbValue->size;

		} else if(ngx_cstr_compare(&attribute->name, "MaxLength")) {
			int32_t maxLength = ngx_atoi(attribute->value.data, attribute->value.len);
			if(maxLength < strbValue->size) {
				if(!strbSetSize(strbValue, maxLength - 3)) return 0;
				strbAppendCString(strbValue, "...");
			}

		} else if(ngx_cstr_compare(&attribute->name, "Format")) {
			//Points to the template data so there is always enough space to add a null termination
			u_char oldChar = *(attribute->value.data + attribute->value.len); //TODO: Make less hacky
			*(attribute->value.data + attribute->value.len) = '\0';
			if(!strbTransform(strbValue, strbTransFormat, attribute->value.data)) return 0;
			*(attribute->value.data + attribute->value.len) = oldChar;

		} else if(ngx_cstr_compare(&attribute->name, "Escape")) {
			if(ngx_cstr_compare(&attribute->value, "UriComponent")) {
				if(!strbTransform(strbValue, strbTransEscapeUri, NGX_ESCAPE_URI_COMPONENT)) return 0;
			} else if(ngx_cstr_compare(&attribute->value, "Uri")) {
				if(!strbTransform(strbValue, strbTransEscapeUri, NGX_ESCAPE_URI)) return 0;
			} else if(ngx_cstr_compare(&attribute->value, "Html")) {
				if(!strbTransform(strbValue, strbTransEscapeHtml)) return 0;
			} else return 0;
		}
		//logHttpDebugMsg1(alsLog, "autols: Attribute %V processed", &attribute->name);
		attribute++;
	}

	return 1;
}
static int applyPatternAppendToken(stringBuilder *strb, alsPatternToken *token, alsConnectionConfig *conConf, alsFileEntry *fileEntry) {
	static stringBuilder strbValue;
	if(!strbValue.isInitialized) if(!strbDefaultInit(&strbValue, 1024, 1024)) return 0;

	stringBuilder *curStrb;
	if(token->attributes.nelts) {
		strbSetSize(&strbValue, 0);
		curStrb = &strbValue;
	} else {
		curStrb = strb;
	}

	//FileEntry Tags
	if(ngx_cstr_compare(&token->name, "EntryIsDirectory")) {
		if(!strbAppendSingle(curStrb, fileEntry->isDirectory ? '1' : '0')) return 0;

	} else if(ngx_cstr_compare(&token->name, "EntryModifiedOn")) {
		ngx_tm_t tm = fileEntry->modifiedOn;
		if(!strbFormat(curStrb, "%02d-%02d-%d %02d:%02d",
			tm.ngx_tm_mday, tm.ngx_tm_mon, tm.ngx_tm_year,
			tm.ngx_tm_hour, tm.ngx_tm_min)) return 0;

	} else if(ngx_cstr_compare(&token->name, "EntrySize")) {
		if(!strbFormat(curStrb, "%ld", fileEntry->size)) return 0;

	} else if(ngx_cstr_compare(&token->name, "EntryName")) {
		if(!strbAppendNgxString(curStrb, &fileEntry->name)) return 0;
		if(fileEntry->isDirectory && !strbAppendSingle(curStrb, '/')) return 0;

	} else //Global Tags
	if(ngx_cstr_compare(&token->name, "RequestUri")) {
		if(!strbAppendNgxString(curStrb, &conConf->request->uri)) return 0;

	} else if(conConf->locConf->keyValuePairs) { //Userdefined Tags (nginx config)
		ngx_keyval_t *kvp = (ngx_keyval_t*)bsearch(&token->name, conConf->locConf->keyValuePairs->elts,
			conConf->locConf->keyValuePairs->nelts, sizeof(ngx_keyval_t), ngxKeyValKeyComparer);

		if(kvp != NULL) if(!strbAppendNgxString(curStrb, &kvp->value)) return 0;
	}

	if(token->attributes.nelts) {
		if(!applyPatternProcessAttributes(&strbValue, &token->attributes, conConf, strb->size)) return 0;
		if(!strbAppendStrb(strb, &strbValue)) return 0;
	}
	return 1;
}
static int applyPatternSectionEnabled(alsConnectionConfig *conConf, ngx_str_t *sectionName) {
	ngx_array_t *sections = conConf->locConf->sections;

	if(!sections) return 0;
	return bsearch(sectionName, sections->elts, sections->nelts, sizeof(ngx_str_t), ngxStringComparer) != NULL;
}
static int applyPatternSub(stringBuilder *strb, ngx_array_t *tokens, alsConnectionConfig *conConf, alsFileEntriesInfo *fileEntriesInfo, alsFileEntry *fileEntry) {
	alsPatternToken *token = (alsPatternToken*)tokens->elts;
	alsPatternToken *tokenLast = token + tokens->nelts - 1;

	for(; token < tokenLast; token++) {
		alsPatternToken *nextToken = token + 1;
		if(!strbAppendMemory(strb, token->end, nextToken->start - token->end)) return 0;
		if(nextToken == tokenLast) break;

		if(ngx_cstr_compare(&nextToken->name, "EntryLoop")) {
			alsFileEntry *fileEntry = (alsFileEntry*)fileEntriesInfo->fileEntries.elts;
			alsFileEntry *fileEntryLimit = fileEntry + fileEntriesInfo->fileEntries.nelts;
			while(fileEntry != fileEntryLimit) {
				conConf->ptnEntryStartPos = strb->size;
				if(!applyPatternSub(strb, &nextToken->children, conConf, fileEntriesInfo, fileEntry++)) return 0;
			}

		} else if(nextToken->children.nelts && applyPatternSectionEnabled(conConf, &nextToken->name)) {
			if(!applyPatternSub(strb, &nextToken->children, conConf, fileEntriesInfo, NULL)) return 0;

		} else {
			if(!applyPatternAppendToken(strb, nextToken, conConf, fileEntry)) return 0;
		}
	}
	return 1;
}
static int applyPattern(stringBuilder *strb, alsPattern *pattern, alsConnectionConfig *conConf, alsFileEntriesInfo *fileEntriesInfo) {
	logHttpDebugMsg0(alsLog, "autols: Applying Pattern");
	return applyPatternSub(strb, &pattern->tokens, conConf, fileEntriesInfo, NULL);
	logHttpDebugMsg0(alsLog, "autols: Pattern applied");
}

static ngx_rc_t setRequestPath(alsConnectionConfig *conConf) {
	size_t     root;

	u_char *last = ngx_http_map_uri_to_path(conConf->request, &conConf->requestPath, &root, STRING_PREALLOCATE);
	if(last == NULL) return NGX_HTTP_INTERNAL_SERVER_ERROR;

	conConf->requestPathCapacity = conConf->requestPath.len;
	conConf->requestPath.len = last - conConf->requestPath.data;

	logHttpDebugMsg1(alsLog, "autols: Request Path \"%V\"", &conConf->requestPath);
	return NGX_OK;
}
static ngx_rc_t openDirectory(alsConnectionConfig *conConf, ngx_dir_t *dir) {
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

		ngx_log_error(level, alsLog, err, ngx_open_dir_n " \"%V\" failed", &conConf->requestPath);

		return rc;
	}
	logHttpDebugMsg0(alsLog, "autols: Directory opened");
	return NGX_OK;
}
static ngx_rc_t closeDirectory(alsConnectionConfig *conConf, ngx_dir_t *dir, int throwError) {
	if(ngx_close_dir(dir) == NGX_ERROR) {
		ngx_log_error(
			NGX_LOG_ALERT, alsLog, ngx_errno,
			ngx_close_dir_n " \"%V\" failed", conConf->requestPath);
	}

	logHttpDebugMsg1(alsLog, "autols: Directory closed (WithErrors=%d)", throwError);
	return !throwError ? NGX_OK : (conConf->request->header_sent ? NGX_ERROR : NGX_HTTP_INTERNAL_SERVER_ERROR);
}
static ngx_rc_t readDirectory(alsConnectionConfig *conConf, ngx_dir_t *dir) {
	ngx_err_t err;

	ngx_set_errno(0);
	if(ngx_read_dir(dir) == NGX_ERROR) {
		err = ngx_errno;

		if(err != NGX_ENOMOREFILES) {
			ngx_log_error(
				NGX_LOG_CRIT, alsLog, err,
				ngx_read_dir_n " \"%V\" failed", conConf->requestPath.data);

			return closeDirectory(conConf, dir, CLOSE_DIRECTORY_ERROR);
		}
		logHttpDebugMsg0(alsLog, "autols: No more files");
		return NGX_DONE;
	}
	return NGX_AGAIN;
}
static ngx_rc_t checkFSEntry(alsConnectionConfig *conConf, ngx_dir_t *dir, u_char *filePath) {
	ngx_err_t err;

	if(dir->valid_info) return NGX_OK;

	logHttpDebugMsg1(alsLog, "autols: Full path: \"%s\"", filePath);
	if(ngx_de_info(filePath, dir) == NGX_FILE_ERROR) {
		err = ngx_errno;

		if(err != NGX_ENOENT && err != NGX_ELOOP) {
			ngx_log_error(NGX_LOG_CRIT, alsLog, err,
				"autols: " ngx_de_info_n " \"%s\" failed", filePath);

			if(err == NGX_EACCES) return NGX_EACCES; //TODO: Make sure there are no flag collisions
			return closeDirectory(conConf, dir, CLOSE_DIRECTORY_ERROR);
		}

		if(ngx_de_link_info(filePath, dir) == NGX_FILE_ERROR) {
			ngx_log_error(NGX_LOG_CRIT, alsLog, ngx_errno,
				"autols: " ngx_de_link_info_n " \"%s\" failed", filePath);

			return closeDirectory(conConf, dir, CLOSE_DIRECTORY_ERROR);
		}
	}

	return NGX_OK;
}
static ngx_rc_t sendHeaders(alsConnectionConfig *conConf, ngx_dir_t *dir) {
	ngx_rc_t rc;

	conConf->request->headers_out.status = NGX_HTTP_OK;
	conConf->request->headers_out.content_type_len = sizeof("text/html") - 1;
	ngx_str_set(&conConf->request->headers_out.content_type, "text/html");

	rc = ngx_http_send_header(conConf->request);
	if(rc == NGX_ERROR || rc > NGX_OK || conConf->request->header_only) {
		if(ngx_close_dir(dir) == NGX_ERROR) {
			ngx_log_error(
				NGX_LOG_ALERT, alsLog,
				ngx_errno, ngx_close_dir_n " \"%V\" failed", conConf->requestPath);
		}
		return rc;
	}

	logHttpDebugMsg0(alsLog, "autols: Header sent");
	return NGX_OK;
}
static ngx_rc_t getFiles(alsConnectionConfig *conConf, ngx_dir_t *dir, alsFileEntriesInfo *fileEntriesInfo) {
	size_t       filePathCapacity;
	ngx_int_t    gmtOffset;
	ngx_str_t    filePath;
	alsFileEntry *entry;
	ngx_rc_t rc;

	gmtOffset = (ngx_timeofday())->gmtoff * 60;
	filePath = conConf->requestPath;
	filePathCapacity = conConf->requestPathCapacity;

	fileEntriesInfo->totalFileNamesLength =
		fileEntriesInfo->totalFileNamesLengthUriEscaped =
		fileEntriesInfo->totalFileNamesLengthHtmlEscaped = 0;

	counters[CounterFileCount] = 0;
	logHttpDebugMsg0(alsLog, "autols: ##Iterating files");
	while((rc = readDirectory(conConf, dir)) == NGX_AGAIN) {
		logHttpDebugMsg1(alsLog, "autols: #File \"%s\"", ngx_de_name(dir));

		//Skip "current directory" path
		if(ngx_de_name(dir)[0] == '.' && ngx_de_name(dir)[1] == '\0') continue;

		//Append filename to request path
		filePath.len = conConf->requestPath.len;
		if(!appendFileName(conConf, dir, &filePath, &filePathCapacity)) {
			return closeDirectory(conConf, dir, CLOSE_DIRECTORY_ERROR);
		}

		rc = checkFSEntry(conConf, dir, filePath.data);
		if(rc == NGX_EACCES) continue;
		if(rc != NGX_OK) return closeDirectory(conConf, dir, CLOSE_DIRECTORY_ERROR);

		//Filter files specified in config
		if(filterFile(&filePath, conConf->locConf->entryIgnores, alsLog)) continue;

		logHttpDebugMsg1(alsLog, "autols: Populating alsFileEntry (Index=%d)", fileEntriesInfo->fileEntries.nelts);
		entry = (alsFileEntry*)ngx_array_push(&fileEntriesInfo->fileEntries);
		if(entry == NULL) return closeDirectory(conConf, dir, CLOSE_DIRECTORY_ERROR);

		entry->name.len = filePath.len - conConf->requestPath.len;
		entry->name.data = (u_char*)ngx_pnalloc(conConf->request->pool, entry->name.len + 1);
		if(entry->name.data == NULL) return closeDirectory(conConf, dir, CLOSE_DIRECTORY_ERROR);
		ngx_cpystrn(entry->name.data, ngx_de_name(dir), entry->name.len + 1);

		//With local time offset if specified
		ngx_gmtime(ngx_de_mtime(dir) + gmtOffset * conConf->locConf->localTime, &entry->modifiedOn);
		entry->isDirectory = ngx_de_is_dir(dir);
		entry->size = ngx_de_size(dir);

		fileEntriesInfo->totalFileNamesLength += filePath.len;
		fileEntriesInfo->totalFileNamesLengthUriEscaped += entry->nameLenAsUri = entry->name.len +
			ngx_escape_uri(NULL, entry->name.data, entry->name.len, NGX_ESCAPE_URI_COMPONENT);
		fileEntriesInfo->totalFileNamesLengthHtmlEscaped += entry->nameLenAsHtml = entry->name.len +
			ngx_escape_html(NULL, entry->name.data, entry->name.len);

		entry->nameLenAsUtf8 = ngx_utf8_length(entry->name.data, entry->name.len);

		logHttpDebugMsg0(alsLog, "autols: File info retrieved");
		counters[CounterFileCount]++;
	}
	if(rc != NGX_DONE) return closeDirectory(conConf, dir, CLOSE_DIRECTORY_ERROR);
	logHttpDebugMsg0(alsLog, "autols: Done iterating files");

	return NGX_OK;
}
static ngx_rc_t createReplyBody(alsConnectionConfig *conConf, alsFileEntriesInfo *fileEntriesInfo, ngx_chain_t **out) {
	alsPattern *pattern = getPattern(&conConf->locConf->patternPath, &conConf->mainConf->patterns);
	if(pattern == NULL) return NGX_ERROR;

	int32_t bufSize = pattern->content.len + 512 * fileEntriesInfo->fileEntries.nelts;
	logHttpDebugMsg1(alsLog, "autols: Estimated buffer size: %d", bufSize);

	stringBuilder strb;
	if(!strbDefaultInit(&strb, bufSize, bufSize)) return NGX_ERROR;

	if(conConf->locConf->printDebug) appendConfig(&strb, conConf);

	if(!applyPattern(&strb, pattern, conConf, fileEntriesInfo)) return NGX_ERROR;

	*out = ngx_alloc_chain_link(conConf->request->connection->pool);
	(*out)->buf = ngx_create_temp_buf(conConf->request->connection->pool, strb.size);

	if(!strbToCString(&strb, (char*)(*out)->buf->last)) return 0;
	strbDispose(&strb);

	(*out)->buf->last += strb.size;
	(*out)->buf->last_in_chain = 1;

	if(conConf->request == conConf->request->main) {
		logHttpDebugMsg0(alsLog, "autols: strb.lastLink->buf->last_buf = 1");
		(*out)->buf->last_buf = 1; //TODO
	}

	return NGX_OK;
}


ngx_rc_t ngx_http_autols_handler(ngx_http_request_t *r) {
	alsFileEntriesInfo fileEntriesInfo;
	alsConnectionConfig  conConf;
	ngx_dir_t         dir;
	ngx_rc_t          rc;

	counters[CounterHandlerInvoke]++;
	if(processHandlerFirstInvokeOn == NULL) {
		processHandlerFirstInvokeOn = (ngx_tm_t*)calloc(1, sizeof(ngx_tm_t));
		ngx_gmtime(ngx_time(), processHandlerFirstInvokeOn);
	}


	ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "autols: Invoked");
	if(r->uri.data[r->uri.len - 1] != '/') return NGX_DECLINED;
	if(!(r->method & (NGX_HTTP_GET | NGX_HTTP_HEAD))) return NGX_DECLINED;

	//Get Settings
	conConf.mainConf = (ngx_http_autols_main_conf_t*)ngx_http_get_module_main_conf(r, ngx_http_autols_module);
	conConf.locConf = (ngx_http_autols_loc_conf_t*)ngx_http_get_module_loc_conf(r, ngx_http_autols_module);
	conConf.request = r;

	alsLog = r->connection->log;

	//Check if we're enabled
	if(!conConf.locConf->enable) {
		logHttpDebugMsg0(alsLog, "autols: Declined request");
		return NGX_DECLINED;
	}
	logHttpDebugMsg0(alsLog, "autols: Accepted request");

	//Get Request Path
	rc = setRequestPath(&conConf);
	if(rc != NGX_OK) return rc;

	//Open Directory
	rc = openDirectory(&conConf, &dir);
	if(rc != NGX_OK) return rc;

	//Send Header
	rc = sendHeaders(&conConf, &dir);
	if(rc != NGX_OK) return rc;

	if(ngx_array_init(&fileEntriesInfo.fileEntries, conConf.request->pool, 40, sizeof(alsFileEntry)) != NGX_OK) {
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
		logHttpDebugMsg1(alsLog, "autols: Sorting %d file entries", fileEntriesInfo.fileEntries.nelts);
		ngx_qsort(fileEntriesInfo.fileEntries.elts,
			(size_t)fileEntriesInfo.fileEntries.nelts,
			sizeof(alsFileEntry), fileEntryComparer);
	}

	//Create Reply Body
	ngx_chain_t *out;
	rc = createReplyBody(&conConf, &fileEntriesInfo, &out);
	if(rc != NGX_OK) return rc;

	rc = ngx_http_output_filter(r, out);

	return rc;
}

void* ngx_http_autols_create_main_conf(ngx_conf_t *cf) {
	ngx_http_autols_main_conf_t *conf;
	conf = (ngx_http_autols_main_conf_t*)ngx_pcalloc(cf->pool, sizeof(ngx_http_autols_main_conf_t));
	if(conf == NULL) return NULL;

	ngx_gmtime(ngx_time(), &conf->createdOn);

	counters[CounterMainCreateCall]++;
	return conf;
}
char* ngx_http_autols_init_main_conf(ngx_conf_t *cf, void *conf) {
	ngx_http_autols_main_conf_t *mainConf = (ngx_http_autols_main_conf_t*)conf;

	ngx_array_init(&mainConf->patterns, cf->pool, 2, sizeof(alsPattern));

	counters[CounterMainMergeCall]++;
	return NGX_CONF_OK;
}

void* ngx_http_autols_create_loc_conf(ngx_conf_t *cf) {
	ngx_http_autols_loc_conf_t  *conf;
	conf = (ngx_http_autols_loc_conf_t*)ngx_pcalloc(cf->pool, sizeof(ngx_http_autols_loc_conf_t));
	if(conf == NULL) return NULL;

	ngx_gmtime(ngx_time(), &conf->createdOn);

	conf->enable = NGX_CONF_UNSET;
	conf->localTime = NGX_CONF_UNSET;
	conf->printDebug = NGX_CONF_UNSET;
	conf->sections = (ngx_array_t*)NGX_CONF_UNSET_PTR;
	conf->entryIgnores = (ngx_array_t*)NGX_CONF_UNSET_PTR;
	conf->keyValuePairs = (ngx_array_t*)NGX_CONF_UNSET_PTR;

	counters[CounterLocCreateCall]++;
	return conf;
}
char* ngx_http_autols_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child) {
	ngx_http_autols_loc_conf_t *prev = (ngx_http_autols_loc_conf_t*)parent;
	ngx_http_autols_loc_conf_t *conf = (ngx_http_autols_loc_conf_t*)child;
	alsLog = cf->log; ngx_uint_t oldLevel = cf->log->log_level; cf->log->log_level |= NGX_LOG_DEBUG_HTTP;

	ngx_conf_merge_value(conf->enable, prev->enable, 0);
	ngx_conf_merge_value(conf->localTime, prev->localTime, 0);
	ngx_conf_merge_value(conf->printDebug, prev->printDebug, 0);
	ngx_conf_merge_str_value(conf->patternPath, prev->patternPath, "");
	ngx_conf_merge_ptr_value(conf->sections, prev->sections, NULL);
	ngx_conf_merge_ptr_value(conf->entryIgnores, prev->entryIgnores, NULL);
	ngx_conf_merge_ptr_value(conf->keyValuePairs, prev->keyValuePairs, NULL);

	ngx_http_autols_main_conf_t *mainConf;
	mainConf = (ngx_http_autols_main_conf_t*)ngx_http_conf_get_module_main_conf(cf, ngx_http_autols_module);

	if(!getPattern(&conf->patternPath, &mainConf->patterns)) {
		alsPattern *pattern;

		if(conf->patternPath.len == 0) {
			logHttpDebugMsg0(alsLog, "autols: Parsing default pattern");
			pattern = (alsPattern*)ngx_array_push(&mainConf->patterns);
			if(pattern == NULL) return "ngx_http_autols_merge_loc_conf: ngx_array_push returned NULL";

			pattern->content.data = (u_char*)defaultPagePattern;
			pattern->content.len = ngx_strlen(defaultPagePattern);
			pattern->path.data = NULL;
			pattern->path.len = 0;

			ngx_array_init(&pattern->tokens, cf->pool, 10, sizeof(alsPatternToken));
			if(!parsePattern(&pattern->tokens, &pattern->content, cf->pool)) {
				return "ngx_http_autols_merge_loc_conf: Couldn't parse default pattern";
			}

		} else {
			//TODO: Read and parse template from disk if found
		}
	}

	if(conf->sections) {
		ngx_qsort(conf->sections->elts, conf->sections->nelts, sizeof(ngx_str_t), ngxStringComparer);
	}
	if(conf->entryIgnores) {
		ngx_qsort(conf->entryIgnores->elts, conf->entryIgnores->nelts, sizeof(ngx_str_t), ngxStringComparer);
	}
	if(conf->keyValuePairs) {
		ngx_qsort(conf->keyValuePairs->elts, conf->keyValuePairs->nelts, sizeof(ngx_str_t), ngxKeyValElemComparer);
	}

	cf->log->log_level = oldLevel;
	counters[CounterLocMergeCall]++;
	return NGX_CONF_OK;
}

ngx_rc_t ngx_http_autols_init(ngx_conf_t *cf) {
	ngx_http_handler_pt        *h;
	ngx_http_core_main_conf_t  *cmcf;
	alsLog = cf->log;

	cmcf = (ngx_http_core_main_conf_t*)ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

	h = (ngx_http_handler_pt*)ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
	if(h == NULL) return NGX_ERROR;

	*h = ngx_http_autols_handler;

	return NGX_OK;
}



#if USE_REGEX
char* ngx_conf_autols_regex_array_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
	u_char regexCompileErrorMsg[NGX_MAX_CONF_ERRSTR];
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
	regexCompile.err.len = NGX_MAX_CONF_ERRSTR;
	regexCompile.pool = cf->pool;

	uint32_t i;
	for(i = 1; i < cf->args->nelts; i++) {
		ngx_str_t *arg = (ngx_str_t*)cf->args->elts + i;
		regexCompile.pattern = *arg;
		if(ngx_regex_compile(&regexCompile) != NGX_OK) {
			ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "%V", &regexCompile.err);
			return (char*)NGX_CONF_ERROR;
		}

		regexEntry = (ngx_regex_elt_t*)ngx_array_push(*dstArray);
		if(regexEntry == NULL) return (char*)NGX_CONF_ERROR;
		regexEntry->regex = regexCompile.regex;
		regexEntry->name = arg->data;
	}

	return NGX_CONF_OK;
}
#endif

char* ngx_conf_autols_set_regex_then_string_array_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
	alsLog = cf->log; cf->log->log_level |= NGX_LOG_DEBUG_HTTP;
	logHttpDebugMsg0(alsLog, "autols: ngx_conf_autols_set_regex_then_string_array_slot");
#if USE_REGEX
	return ngx_conf_autols_regex_array_slot(cf, cmd, conf);
#else
	return ngx_conf_autols_set_str_array_slot(cf, cmd, conf);
#endif
	logHttpDebugMsg0(alsLog, "autols: /ngx_conf_autols_set_regex_then_string_array_slot");
}

char* ngx_conf_autols_set_keyval_array_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
	alsLog = cf->log; cf->log->log_level |= NGX_LOG_DEBUG_HTTP;

	logHttpDebugMsg0(alsLog, "autols: ngx_conf_autols_set_keyval_array_slot");
	ngx_array_t **a = (ngx_array_t **)((char*)conf + cmd->offset);
	if(*a == NGX_CONF_UNSET_PTR) {
		*a = ngx_array_create(cf->pool, 4, sizeof(ngx_keyval_t));
		if(*a == NULL) {
			return (char*)NGX_CONF_ERROR;
		}
	}

	uint32_t i;
	for(i = 1; i < cf->args->nelts; i++) {
		ngx_str_t *arg = (ngx_str_t*)cf->args->elts + i;
		u_char* equalPos = (u_char*)ngx_strchr(arg->data, '=');
		if(equalPos == NULL) return (char*)NGX_CONF_ERROR;

		ngx_keyval_t *pair = (ngx_keyval_t*)ngx_array_push(*a);
		if(pair == NULL) return (char*)NGX_CONF_ERROR;

		pair->key.data = arg->data;
		pair->key.len = equalPos - arg->data;
		pair->value.data = equalPos + 1;
		pair->value.len = (arg->data + arg->len) - (equalPos + 1);
		logHttpDebugMsg2(alsLog, "(%V = %V)", &pair->key, &pair->value);
	}

	if(cmd->post) {
		ngx_conf_post_t *post = (ngx_conf_post_t*)cmd->post;
		return post->post_handler(cf, post, a);
	}

	logHttpDebugMsg0(alsLog, "autols: /ngx_conf_autols_set_keyval_array_slot");
	return NGX_CONF_OK;
}



char* ngx_conf_autols_set_str_array_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
	alsLog = cf->log; cf->log->log_level |= NGX_LOG_DEBUG_HTTP;

	logHttpDebugMsg0(alsLog, "autols: ngx_conf_autols_set_str_array_slot");
	ngx_array_t **a = (ngx_array_t **)((char*)conf + cmd->offset);
	if(*a == NGX_CONF_UNSET_PTR) {
		*a = ngx_array_create(cf->pool, 4, sizeof(ngx_str_t));
		if(*a == NULL) {
			return (char*)NGX_CONF_ERROR;
		}
	}

	uint32_t i;
	for(i = 1; i < cf->args->nelts; i++) {
		ngx_str_t *arg = (ngx_str_t*)cf->args->elts + i;
		ngx_str_t *str = (ngx_str_t*)ngx_array_push(*a);
		if(str == NULL) return (char*)NGX_CONF_ERROR;

		*str = *arg;
		logHttpDebugMsg1(alsLog, "%V", str);
	}

	if(cmd->post) {
		ngx_conf_post_t *post = (ngx_conf_post_t*)cmd->post;
		return post->post_handler(cf, post, a);
	}

	logHttpDebugMsg0(alsLog, "autols: /ngx_conf_autols_set_str_array_slot");
	return NGX_CONF_OK;
}
