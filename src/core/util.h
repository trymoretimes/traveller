#ifndef __CORE_UTIL_H
#define __CORE_UTIL_H

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <locale.h>

/* Anti-warning macro... */
#define NOTUSED(V) ((void) V)

#define ERRNO_OK    0
#define ERRNO_ERR   -500
#define ERRNO_NULL  -404

#define trvLog_ERROR 1
#define trvLog_WARNING 2
#define trvLog_NOTICE 3
#define trvLog_INFO 4
#define trvLog_DEBUG 6

#define trvLog(level, FMT, ...) do {\
    char timestr[64];\
    struct timeval tv;\
    gettimeofday(&tv,NULL);\
    strftime(timestr,sizeof(timestr),"%d %b %H:%M:%S.",localtime(&tv.tv_sec));\
    fputc('[', stderr); fputs(timestr, stderr); fputs("]", stderr); \
    fprintf(stderr, "(%s:%d) ", __FILE__, __LINE__); fprintf(stderr, FMT, ##__VA_ARGS__); \
    fputc('\n', stderr); \
} while(0);

#define trvLogD(FMT, ...) trvLog(trvLog_DEBUG, FMT, ##__VA_ARGS__)
#define trvLogW(FMT, ...) trvLog(trvLog_WARNING, FMT, ##__VA_ARGS__)
#define trvLogE(FMT, ...) trvLog(trvLog_ERROR, FMT, ##__VA_ARGS__)
#define trvLogN(FMT, ...) trvLog(trvLog_NOTICE, FMT, ##__VA_ARGS__)
#define trvLogI(FMT, ...) trvLog(trvLog_INFO, FMT, ##__VA_ARGS__)

#ifdef IS_DEBUG
#include <assert.h>
#define trvAssert(condition) assert(condition);
#else
#define trvAssert(condition) {}
#endif


#define run_with_period(_ms_) if ((_ms_ <= 1000/server.hz) || !(server.cronloops%((_ms_)/(1000/server.hz))))


int dictSdsKeyCaseCompare(void *privdata, const void *key1,
        const void *key2);
void dictSdsDestructor(void *privdata, void *val);
unsigned int dictSdsCaseHash(const void *key);
unsigned int dictStringCaseHash(const void *key);
int dictStringCompare(void *privdata, const void *key1,
        const void *key2);

#define ALLOW_PATH_SIZE 256

#define trvExit(ERRNO, FMT, ...) do { \
    trvLog(trvLog_ERROR, FMT, ##__VA_ARGS__); \
    exit(ERRNO); \
} while(0);


#endif