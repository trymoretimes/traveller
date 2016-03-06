#include <string.h>

#include "core/zmalloc.h"
#include "core/adlist.h"
#include "core/dict.h"
#include "core/sds.h"
#include "core/util.h"
#include "core/extern.h"

#include "event/event.h"
#include "ui/ui.h"
#include "ui/document/document.h"

#include "core/extern.h"
#include "ui/extern.h"
#include "ui/document/extern.h"

static dict *UIHtmlSpecialStringTable;

static dict *uiHtmlDomInfoDict;
static uiHtmlDomInfo_t uiHtmlDomInfoTable[] = {
    {"unknown", UIHTML_DOM_TYPE_UNKNOWN},
    {"text",    UIHTML_DOM_TYPE_TEXT},
    {"html",    UIHTML_DOM_TYPE_HTML},
    {"head",    UIHTML_DOM_TYPE_HEAD},
    {"title",   UIHTML_DOM_TYPE_TITLE},
    {"body",    UIHTML_DOM_TYPE_BODY},
    {"script",  UIHTML_DOM_TYPE_SCRIPT},
    {"div",     UIHTML_DOM_TYPE_DIV},
    {"table",   UIHTML_DOM_TYPE_TABLE},
    {"tr",      UIHTML_DOM_TYPE_TR},
    {"td",      UIHTML_DOM_TYPE_TD},
    {"style",   UIHTML_DOM_TYPE_STYLE},
    {"input",   UIHTML_DOM_TYPE_INPUT},
    {0, 0},
};

static inline void skipStringNotConcern(char **ptr)  {
    while (UI_IsWhiteSpace(**ptr) && 0 != **ptr) {
        (*ptr)++;
    }
}

int IsHtmlDomNotCareCssDeclaration(enum uiHtmlDomType_e type) {
    if (UIHTML_DOM_TYPE_TEXT == type ||
            UIHTML_DOM_TYPE_SCRIPT == type ||
            UIHTML_DOM_TYPE_STYLE == type) {
        return 1;
    } else {
        return 0;
    }
}

void UI_PrepareHtml() {
    uiHtmlDomInfoDict = dictCreate(&stackStringTableDictType, 0);
    for (uiHtmlDomInfo_t *domInfo = &uiHtmlDomInfoTable[0]; 0 != domInfo->name; domInfo++) {
        dictAdd(uiHtmlDomInfoDict, domInfo->name, domInfo);
    }

    UIHtmlSpecialStringTable = dictCreate(&stackStringTableDictType, 0);
    dictAdd(UIHtmlSpecialStringTable, "&quot;", "\"");
    dictAdd(UIHtmlSpecialStringTable, "&amp;",  "&");
    dictAdd(UIHtmlSpecialStringTable, "&gt;",   ">");
    dictAdd(UIHtmlSpecialStringTable, "&lt;",   "<");
    dictAdd(UIHtmlSpecialStringTable, "&nbsp;", " ");
}

uiDocumentScanToken_t* UI_ScanHtmlToken(uiDocumentScanner_t *scanner) {
    char **ptr = &scanner->current;

    skipStringNotConcern(ptr);

    if ('\0' == **ptr) {
        return 0;
    }

    int tokenStack[6] = {0};
    int tokenStackLen = 0;

    char *s = *ptr;
    int len;
    
    uiDocumentScanToken_t *token = UI_NewDocumentScanToken();
    token->type = UIHTML_TOKEN_TEXT;

    while ('\0' != *s && tokenStackLen < 6) {
        // 解析覆盖一下情况
        // "<text>"
        // "<text/>"
        // "</text>"
        // "text"
        // "<text<text>"

        switch (*s) {
            case '<':
                //预防 "< lkasfd < >" 的情况
                if (0 == tokenStackLen) {
                    tokenStack[0] = '<';
                    tokenStackLen++;
                } else {
                    if ('<' == tokenStack[0]) {
                        if (UIHTML_TOKEN_TEXT != tokenStack[tokenStackLen-1]) {
                            tokenStack[tokenStackLen] = UIHTML_TOKEN_TEXT;
                            tokenStackLen++;
                        }
                    } else {
                        tokenStack[tokenStackLen] = '<';
                        tokenStackLen++;
                    }
                }
                break;
            case '>':
                tokenStack[tokenStackLen] = '>';
                tokenStackLen++;
                break;
            case '/':
                tokenStack[tokenStackLen] = '/';
                tokenStackLen++;
                break;
            default:
                if (0 == tokenStackLen) {
                    tokenStack[0] = UIHTML_TOKEN_TEXT;
                    tokenStackLen++;
                } else if (UIHTML_TOKEN_TEXT != tokenStack[tokenStackLen-1]) {
                    tokenStack[tokenStackLen] = UIHTML_TOKEN_TEXT;
                    tokenStackLen++;
                }
        }

        if (3 == tokenStackLen && 
                '<' == tokenStack[0]  &&   // <
                UIHTML_TOKEN_TEXT == tokenStack[1] &&   // tag
                '>' == tokenStack[2]) {    // >
            token->type = UIHTML_TOKEN_START_TAG;
            goto GET_TOKEN_SUCCESS;

        } else if (4 == tokenStackLen) {
            if ('<' == tokenStack[0]      && // <
                    '/' == tokenStack[1]  && // /
                    UIHTML_TOKEN_TEXT == tokenStack[2] && // tag
                    '>' == tokenStack[3]) {    // >
                token->type = UIHTML_TOKEN_END_TAG;
                goto GET_TOKEN_SUCCESS;

            } else if ('<' == tokenStack[0] && // <
                    UIHTML_TOKEN_TEXT == tokenStack[1]   && // tag
                    '/' == tokenStack[2]    && // /
                    '>' == tokenStack[3]) {    // >
                token->type = UIHTML_TOKEN_SELF_CLOSING_TAG;
                goto GET_TOKEN_SUCCESS;
            }

        } else if (2 == tokenStackLen && 
                UIHTML_TOKEN_TEXT == tokenStack[0] &&    // text
                '<' == tokenStack[1]) {     // <
            token->type = UIHTML_TOKEN_TEXT;
            tokenStackLen--;
            s--;
            goto GET_TOKEN_SUCCESS;
        }

        s++;
    }

GET_TOKEN_SUCCESS:

    len = (int)(s-*ptr+1);

    //去掉尾部空格
    if (UIHTML_TOKEN_TEXT == token->type) {
        s = &((*ptr)[len-1]);
        for (int i = len-1; i >= 0; i--,s--) {
            if (UI_IsWhiteSpace(*s)) {
                continue;
            }
            break;
        }
    }

    token->content = sdscatlen(token->content, *ptr, (int)(s-*ptr+1));
    (*ptr) = (*ptr) + len;
    return token;
}

uiHtmlDom_t* UI_NewHtmlDom(uiHtmlDom_t *parentDom) {
    uiHtmlDom_t *dom = (uiHtmlDom_t*)zmalloc(sizeof(uiHtmlDom_t));
    memset(dom, 0, sizeof(uiHtmlDom_t));
    dom->parent = parentDom;
    dom->title = sdsempty();
    dom->type = UIHTML_DOM_TYPE_UNKNOWN;
    dom->children = listCreate();
    dom->children->free = UI_FreeHtmlDom;
    dom->renderObject = UI_newDocumentRenderObject(dom);
    return dom;
}

void UI_FreeHtmlDom(void *_dom) {
    uiHtmlDom_t *dom = (uiHtmlDom_t*)_dom;
    sdsfree(dom->title);
    if (0 != dom->attributes) listRelease(dom->attributes);
    if (0 != dom->id) sdsfree(dom->id);
    if (0 != dom->classes) listRelease(dom->classes);
    if (0 != dom->content) sdsfree(dom->content);
    listRelease(dom->children);
}

static inline void parseHtmlDomStyle(uiHtmlDom_t *dom, char *stylePtr) {
    enum {EXPECTING_KEY, EXPECTING_VALUE} expectState;

    sds data = sdsMakeRoomFor(sdsempty(), strlen(stylePtr));
    uiCssDeclaration_t *cssDeclaration;
    sds cssDeclarationKey = 0;
    sds cssDeclarationValue = 0;

    expectState = EXPECTING_KEY;
    int offset;
    while (1) {
        if (UI_IsWhiteSpace(*stylePtr)) {
            stylePtr++;
        }
        
        if ('\'' == *stylePtr || '"' == *stylePtr) {
            escapeQuoteContent(data, &stylePtr);

        } else {
            offset = -1;
            stylePtr--;
            while(1) {
                stylePtr++;
                offset++;
                if ('\0' == *stylePtr) {
                    break;
                }

                if (UI_IsWhiteSpace(*stylePtr) ||
                        ';' == *stylePtr ||
                        ':' == *stylePtr) {
                    stylePtr++;
                    break;
                } 

                data[offset] = *stylePtr;
            }
            data[offset] = '\0';
        }


        switch (expectState) {
            case EXPECTING_KEY:
                expectState = EXPECTING_VALUE;
                cssDeclarationKey = sdsnewlen(data, strlen(data));
                break;

            case EXPECTING_VALUE:
                expectState = EXPECTING_KEY;
                cssDeclarationValue = sdsnewlen(data, strlen(data));

                if (0 == dom->styleCssDeclarations) {
                    dom->styleCssDeclarations = listCreate();
                    dom->styleCssDeclarations->free = UI_FreeCssDeclaration;
                }
                cssDeclaration = UI_NewCssDeclaration();
                UI_ParseSdsToCssDeclaration(cssDeclaration, cssDeclarationKey, cssDeclarationValue);
                dom->styleCssDeclarations = listAddNodeTail(dom->styleCssDeclarations, cssDeclaration);

                cssDeclarationKey = 0;
                cssDeclarationValue = 0;

                break;
        }

        if ('\0' == *stylePtr) {
            break;
        }
    }

    if (0 != cssDeclarationKey) sdsfree(cssDeclarationKey);
    if (0 != cssDeclarationValue) sdsfree(cssDeclarationValue);
    sdsfree(data);
}

// 解析单个tag 中的 title和attribute
static inline void parseHtmlDomTag(uiHtmlDom_t *dom, uiDocumentScanToken_t *token) {
    int contentLen = sdslen(token->content);

    int contentOffset;
    int contentEndOffset;
    enum {EXPECTING_KEY, EXPECTING_VALUE} expectState;

    // 提取 title
    contentOffset = 0;
    for (contentEndOffset = 1; contentEndOffset < contentLen; contentEndOffset++) {
        if (UI_IsWhiteSpace(token->content[contentEndOffset])) {
            break;
        }
    }
    dom->title = sdscatlen(dom->title, &(token->content[contentOffset]), contentEndOffset-contentOffset);
    uiHtmlDomInfo_t *domInfo = dictFetchValue(uiHtmlDomInfoDict, dom->title);
    if (0 == domInfo) {
        dom->type = UIHTML_DOM_TYPE_UNKNOWN;
    } else {
        dom->type = domInfo->type;
    }

    contentOffset = contentEndOffset + 1;

    int len = strlen(&token->content[contentOffset]);
    if (0 == len) {
        return;
    }

    doubleString_t *domAttribute;
    sds attributeData = sdsnewlen(&token->content[contentOffset], len);
    int attributeDataOffset;
    char *attributeDataPtr = &token->content[contentOffset];
    char *attributeKey = 0;
    char *attributeValue = 0;
    char *sptr, *sptr2;

    // 提取attribute
    expectState = EXPECTING_KEY;
    while (1) {
        // 开始解析新的 key 或 value

        // 跳过空格
        if (UI_IsWhiteSpace(*attributeDataPtr)) {
            attributeDataPtr++;
        }

        if ('\'' == *attributeDataPtr || '"' == *attributeDataPtr) {
            escapeQuoteContent(attributeData, &attributeDataPtr);

        } else {
            attributeDataOffset = -1;
            attributeDataPtr--;
            while (1) {
                attributeDataOffset++;
                attributeDataPtr++;

                if ('\0' == *attributeDataPtr) {
                    break;
                }

                if (UI_IsWhiteSpace(*attributeDataPtr)) {
                    attributeDataPtr++;
                    break;
                }

                if ('=' == *attributeDataPtr) {
                    attributeDataPtr++;
                    break;
                }

                if ('\\' == *attributeDataPtr) {
                    attributeDataPtr++;
                    attributeData[attributeDataOffset] = *attributeDataPtr;
                    continue;
                }

                attributeData[attributeDataOffset] = *attributeDataPtr;
            }
            attributeData[attributeDataOffset] = '\0';
        }

        switch (expectState) {
            case EXPECTING_KEY:
                attributeKey = stringnewlen(attributeData, strlen(attributeData));
                expectState = EXPECTING_VALUE;
                break;
            case EXPECTING_VALUE:
                attributeValue = stringnewlen(attributeData, strlen(attributeData));
                expectState = EXPECTING_KEY;

                if (0 == dom->attributes) {
                    dom->attributes = listCreate();
                    dom->attributes->free = freeDoubleString;
                }

                domAttribute = newDoubleString();
                domAttribute->v1 = attributeKey;
                domAttribute->v2 = attributeValue;
                dom->attributes = listAddNodeTail(dom->attributes, domAttribute);

                if (0 == stringcmp("class", attributeKey)) {
                    if (0 == dom->classes) {
                        dom->classes = listCreate();
                        dom->classes->free = listFreeSds;
                    }

                    sptr = attributeValue;
                    while (UI_IsWhiteSpace(*sptr)) {
                        sptr++;
                    }
                    sptr2 = sptr + 1;
                    while (1) {
                        if ('\0' == *sptr2) {
                            break;
                        }

                        if (UI_IsWhiteSpace(*sptr2)) {
                            listAddNodeTail(dom->classes, sdsnewlen(sptr, sptr2-sptr));

                            while (UI_IsWhiteSpace(*sptr2)) {
                                sptr2++;
                            }

                            sptr = sptr2;

                            if ('\0' == *sptr2) {
                                break;
                            }
                        }

                        sptr2++;
                    }

                    if (sptr2 > sptr) {
                        listAddNodeTail(dom->classes, sdsnewlen(sptr, sptr2-sptr));
                    }

                } else if (0 == stringcmp("id", attributeKey)) {
                    if (0 != dom->id) {
                        sdsfree(dom->id);
                    }

                    dom->id = sdsnew(attributeValue);

                } else if (0 == stringcmp("style", attributeKey)) {
                    parseHtmlDomStyle(dom, attributeValue);
                }

                attributeKey = 0;
                attributeValue = 0;
                break;
        }

        if ('\0' == *attributeDataPtr) {
            goto PARSE_HTML_DOM_TAG_END;
        }
    }

PARSE_HTML_DOM_TAG_END:
    if (0 != attributeData) sdsfree(attributeData);
    if (0 != attributeKey) zfree(attributeKey);
    if (0 != attributeValue) zfree(attributeValue);
}

// @return uiHtmlDom_t* 需要继续处理的 uiHtmlDom_t*

// <tag>
static inline uiHtmlDom_t* parseHtmlTokenStartTag(uiHtmlDom_t *dom, uiDocumentScanToken_t *token) {
    sdsrange(token->content, 1, -2);
    uiHtmlDom_t *newdom = UI_NewHtmlDom(dom);
    parseHtmlDomTag(newdom, token);
    dom->children = listAddNodeTail(dom->children, newdom);
    return newdom;
}

// </tag>
static inline uiHtmlDom_t* parseHtmlTokenEndTag(uiHtmlDom_t *dom, uiDocumentScanToken_t *token) {
    sdsrange(token->content, 2, -2);
    return dom->parent;
}

// <tag/>
static inline uiHtmlDom_t* parseHtmlTokenSelfClosingTag(uiHtmlDom_t *dom, uiDocumentScanToken_t *token) {
    sdsrange(token->content, 1, -3);
    uiHtmlDom_t *newdom = UI_NewHtmlDom(dom);
    parseHtmlDomTag(newdom, token);
    dom->children = listAddNodeTail(dom->children, newdom);
    return dom;
}

// text
static inline uiHtmlDom_t* parseHtmlTokenText(uiDocument_t *document,
        uiHtmlDom_t *dom, uiDocumentScanToken_t *token) {

    if (UIHTML_DOM_TYPE_SCRIPT == dom->type) {
        document->script = sdscatsds(document->script, token->content);

    } else if (UIHTML_DOM_TYPE_STYLE == dom->type) {
        document->style = sdscatsds(document->style, token->content);

    } else {
        // 除了 script 以外的dom内容，均作特殊处理
        uiHtmlDom_t *newdom = UI_NewHtmlDom(dom);
        newdom->content = sdsempty();
        dom->children = listAddNodeTail(dom->children, newdom);

        newdom->type = UIHTML_DOM_TYPE_TEXT;
        newdom->content = sdsMakeRoomFor(newdom->content, 
                sdslen(newdom->content)+sdslen(token->content));

        int domPoi = sdslen(newdom->content);
        int isWhiteSpaceFound = 0;
        int len = sdslen(token->content);
        char *ptr = token->content;
        char *ptr2;
        sds tmpString = sdsMakeRoomFor(sdsempty(), 12);
        for (int i = 0; i < len; i++,ptr++) {
            //合并空格
            if (UI_IsWhiteSpace(*ptr)) {
                if (0 == isWhiteSpaceFound) {
                    isWhiteSpaceFound = 1;
                    newdom->content[domPoi] = ' ';
                    domPoi++;
                }
                continue;

            }

            if (1 == isWhiteSpaceFound) {
                isWhiteSpaceFound = 0;
            }

            //转义
            if ('&' == *ptr) {
                ptr2 = ptr;
                while (i < len && ';' != *ptr2) {
                    i++;ptr2++;
                }
                sdsclear(tmpString);
                tmpString = sdscatlen(tmpString, ptr, ptr2-ptr+1);
                ptr = ptr2;
                ptr2 = dictFetchValue(UIHtmlSpecialStringTable, tmpString);
                if (0 != ptr2) {
                    newdom->content[domPoi] = *ptr2;
                    domPoi++;
                }
                continue;
            }

            newdom->content[domPoi] = *ptr;
            domPoi++;
        }
        sdsfree(tmpString);
        newdom->content[domPoi] = '\0';
        newdom->contentUtf8Width = utf8StrWidth(newdom->content);
        sdsupdatelen(newdom->content);
    }

    return dom;
}

int UI_ParseHtml(uiDocument_t *document) {
    document->rootDom = UI_NewHtmlDom(0);
    uiHtmlDom_t *dom = document->rootDom;
    uiDocumentScanner_t htmlScanner = {
        0, document->content, document->content, UI_ScanHtmlToken
    };

    uiDocumentScanToken_t *token;
    while (0 != (token = htmlScanner.scan(&htmlScanner))) {
        switch (token->type) {
            case UIHTML_TOKEN_START_TAG:
                dom = parseHtmlTokenStartTag(dom, token);
                break;
            case UIHTML_TOKEN_END_TAG:
                dom = parseHtmlTokenEndTag(dom, token);
                break;
            case UIHTML_TOKEN_SELF_CLOSING_TAG:
                dom = parseHtmlTokenSelfClosingTag(dom, token);
                break;
            case UIHTML_TOKEN_TEXT:
                dom = parseHtmlTokenText(document, dom, token);
                break;
        }

        UI_FreeDocumentScanToken(token);
    }

    return 0;
}

void UI_PrintHtmlDomTree(uiHtmlDom_t *dom, int indent) {
    int i;
    for (i = 0; i < indent; i++) { printf("  "); }
    if (UIHTML_DOM_TYPE_TEXT != dom->type) {
        printf("<%s>", dom->title);
    }
    if (0 != dom->attributes && listLength(dom->attributes) > 0) {
        listIter *liAttribute;
        listNode *lnAttribute;
        doubleString_t *attribute;
        liAttribute = listGetIterator(dom->attributes, AL_START_HEAD);
        while (0 != (lnAttribute = listNext(liAttribute))) {
            attribute = (doubleString_t*)listNodeValue(lnAttribute);
            printf(" |%s=%s| ", attribute->v1, attribute->v2);
        }
        listReleaseIterator(liAttribute);
    }

    if (UIHTML_DOM_TYPE_TEXT == dom->type) {
        printf("%s\n", dom->content);

    } else {
        printf("\n");
    }

    listIter *li;
    listNode *ln;
    li = listGetIterator(dom->children, AL_START_HEAD);
    indent++;
    while (0 != (ln = listNext(li))) {
        UI_PrintHtmlDomTree((uiHtmlDom_t*)listNodeValue(ln), indent);
    }
    listReleaseIterator(li);
}
