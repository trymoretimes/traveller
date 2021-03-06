#ifndef __NET_NETWORKING_H
#define __NET_NETWORKING_H

#define ANET_OK 0
#define ANET_ERR -1
#define ANET_ERR_LEN 256

/* Flags used with certain functions. */
#define ANET_NONE 0
#define ANET_IP_ONLY (1<<0)

#if defined(__sun)
#define AF_LOCAL AF_UNIX
#endif

int anetTcpConnect(char *err, char *addr, int port);
int anetTcpNonBlockConnect(char *err, char *addr, int port);
int anetTcpNonBlockBindConnect(char *err, char *addr, int port, char *source_addr);
int anetUnixConnect(char *err, char *path);
int anetUnixNonBlockConnect(char *err, char *path);
int anetRead(int fd, char *buf, int count);
int anetResolve(char *err, char *host, char *ipbuf, size_t ipbuf_len);
int anetResolveIP(char *err, char *host, char *ipbuf, size_t ipbuf_len);
int anetTcpServer(char *err, int port, char *bindaddr, int backlog);
int anetTcp6Server(char *err, int port, char *bindaddr, int backlog);
int anetUnixServer(char *err, char *path, mode_t perm, int backlog);
int anetTcpAccept(char *err, int serversock, char *ip, size_t ip_len, int *port);
int anetUnixAccept(char *err, int serversock);
int anetWrite(int fd, char *buf, int count);
int anetNonBlock(char *err, int fd);
int anetEnableTcpNoDelay(char *err, int fd);
int anetDisableTcpNoDelay(char *err, int fd);
int anetTcpKeepAlive(char *err, int fd);
int anetPeerToString(int fd, char *ip, size_t ip_len, int *port);
int anetKeepAlive(char *err, int fd, int interval);
int anetSockName(int fd, char *ip, size_t ip_len, int *port);
int anetPeerSocket(char *err, int port, char *bindaddr, int af);
int anetPeerConnect(int fd, char *err, char *addr, int port);
int anetPeerListen(int fd, char *err, int backlog);

#define AE_OK 0
#define AE_ERR -1

#define AE_NONE 0
#define AE_READABLE 1
#define AE_WRITABLE 2

#define AE_FILE_EVENTS 1
#define AE_TIME_EVENTS 2
#define AE_ALL_EVENTS (AE_FILE_EVENTS|AE_TIME_EVENTS)
#define AE_DONT_WAIT 4

#define AE_NOMORE -1

/* Macros */
#define AE_NOTUSED(V) ((void) V)

struct aeLooper_s;

/* Types and data structures */
typedef void aeFileProc(struct aeLooper_s *eventLoop, int fd, void *clientData, int mask);
typedef int aeTimeProc(struct aeLooper_s *eventLoop, long long id, void *clientData);
typedef void aeEventFinalizerProc(struct aeLooper_s *eventLoop, void *clientData);
typedef void aeBeforeSleepProc(struct aeLooper_s *eventLoop);

/* File event structure */
typedef struct aeFileEvent_s {
    int        mask; /* one of AE_(READABLE|WRITABLE) */
    aeFileProc *RfileProc;
    aeFileProc *WfileProc;
    void       *ClientData;
} aeFileEvent_t;

/* Time event structure */
typedef struct aeTimeEvent_s {
    long long            id; /* time event identifier. */
    long                 whenSec; /* seconds */
    long                 whenMs; /* milliseconds */
    aeTimeProc           *timeProc;
    aeEventFinalizerProc *finalizerProc;
    struct aeTimeEvent_s *next;
    void                 *ClientData;
} aeTimeEvent_t;

/* A fired event */
typedef struct aeFiredEvent_s {
    int fd;
    int mask;
} aeFiredEvent_t;

/* State of an event based program */
typedef struct aeLooper_s {
    int               maxfd;   /* highest file descriptor currently registered */
    int               setsize; /* max number of file descriptors tracked */
    long long         timeEventNextId;
    time_t            lastTime;     /* Used to detect system clock skew */
    aeFileEvent_t     *events; /* Registered events */
    aeFiredEvent_t    *fired; /* Fired events */
    aeTimeEvent_t     *timeEventHead;
    int               stop;
    void              *apidata; /* This is used for polling API specific data */
    aeBeforeSleepProc *beforesleep;
} aeLooper_t;

/* Prototypes */
aeLooper_t *aeNewLooper(int setsize);
void aeStop(aeLooper_t *eventLoop);
int aeCreateFileEvent(aeLooper_t *eventLoop, int fd, int mask,
        aeFileProc *proc, void *clientData);
void aeDeleteFileEvent(aeLooper_t *eventLoop, int fd, int mask);
int aeGetFileEvents(aeLooper_t *eventLoop, int fd);
long long aeCreateTimeEvent(aeLooper_t *eventLoop, long long milliseconds,
        aeTimeProc *proc, void *clientData,
        aeEventFinalizerProc *finalizerProc);
int aeDeleteTimeEvent(aeLooper_t *eventLoop, long long id);
int aeProcessEvents(aeLooper_t *eventLoop, int flags);
int aeWait(int fd, int mask, long long milliseconds);
char *aeGetApiName(void);
void aeSetBeforeSleepProc(aeLooper_t *eventLoop, aeBeforeSleepProc *beforesleep);
int aeGetSetSize(aeLooper_t *eventLoop);
int aeResizeSetSize(aeLooper_t *eventLoop, int setsize);
void aeDeleteLooper(aeLooper_t *eventLoop);
void aeMain(aeLooper_t *eventLoop);
void* aeMainDeviceWrap(void *_eventLoop);

/* networking，分装了所有的网络操作
 */

#define TRV_NET_TCP_BACKLOG 1024
#define TRV_NET_MAX_SNODE 1024 * 24
#define TRV_NET_TCPKEEPALIVE 0 
#define TRV_NET_MAX_ACCEPTS_PER_CALL 1000
#define TRV_NET_IOBUF_LEN (1024)  // Generic I/O buffer size 

/* 基于RESP通信协议，不过有点不一样
 * 通信ntRespSnode_t接受信息各个状态如下：
 */
#define SNODE_RECV_STAT_ACCEPT   0  // 客户端首次连接，socket accept后 
#define SNODE_RECV_STAT_PREPARE  1  // 准备就绪，等待数据中 
#define SNODE_RECV_STAT_PARSING  3  // 解析数据中 
#define SNODE_RECV_STAT_PARSED   4  // 数据完成 
#define SNODE_RECV_STAT_EXCUTING 5  // 正在执行中 
#define SNODE_RECV_STAT_EXCUTED  6  // 命令执行完成 
#define NTResp_SnodeServiceSetFinishedFlag(sn) sn->recvStat = SNODE_RECV_STAT_EXCUTED;

/* 各个类型解析状态
 */
#define SNODE_RECV_STAT_PARSING_START       0 
#define SNODE_RECV_STAT_PARSING_ARGC        1
#define SNODE_RECV_STAT_PARSING_ARGV_NUM    2
#define SNODE_RECV_STAT_PARSING_ARGV_VALUE  3
#define SNODE_RECV_STAT_PARSING_FINISHED    4

/* 对于traveller的网络库来说，每个与travler连接的socket，都将分装成 ntRespSnode_t (socket node)，
 * 包括连接traveller的client，或traveller主动连接的nt_RespServer
 */
#define SNODE_MAX_QUERYBUF_LEN (1024*1024*1024) // 1GB max query buffer. 
#define SNODE_CLOSE_AFTER_REPLY (1<<0)  // 发送完信息后，断开连接 

typedef struct ntScriptServiceRequestCtx_s {
    int requestId;
    lua_State *ScriptServiceLua;
    sds ScriptServiceCallbackUrl;
    sds ScriptServiceCallbackArg;
} ntScriptServiceRequestCtx_t;

typedef struct ntRespSnode_s ntRespSnode_t;
typedef struct ntRespSnode_s {
    int flags;              // SNODE_CLOSE_AFTER_REPLY | ... 
    int fd;
    char fdstr[16];           // 字符串类型的fd 
    int recvStat;          // SNODE_RECV_STAT_ACCEPT ... 
    int recvType;          // SNODE_RECV_TYPE_ERR ... 
    sds tmpQuerybuf;       // 临时存放未解析完的argc 或 参数长度 
    sds querybuf;           // 读取到的数据 
    sds writebuf;           // 等待发送的数据 
    int recvParsingStat;
    int Argc;

    sds *Argv;
    int ArgvSize;

    int argcRemaining;     // 还剩多少个argc没有解析完成 
    int argvRemaining;     // 正在解析中的参数还有多少字符未获取 -1 为还没开始解析
    time_t lastinteraction; // time of the last interaction, used for timeout 

    void (*responseProc) (ntRespSnode_t *sn); //在等待远程机器发来结果的回调函数

    int ScriptServiceRequestCtxListMaxId;
    list *ScriptServiceRequestCtxList;

    void (*Proc) (ntRespSnode_t *sn);
    void (*hupProc) (ntRespSnode_t *sn); //如果远程机器挂掉了，需要调用的函数

    int isWriteMod;       // 是否已处于写数据模式，避免重复进入写数据模式 
} ntRespSnode_t;

#define SNODE_RECV_TYPE_HUP    -2 // 远程机器挂掉了
#define SNODE_RECV_TYPE_ERR    -1 // -:ERR 
#define SNODE_RECV_TYPE_OK     1  // +:OK 
#define SNODE_RECV_TYPE_STRING 2
#define SNODE_RECV_TYPE_ARRAY  3  // 数组 且 命令 

typedef struct ntServer_s {
    time_t unixtime;        // Unix time sampled every cron cycle. 

    int maxSnodes;
    char *bindaddr;
    int port;
    int tcpBacklog;
    char neterr[ANET_ERR_LEN];   // Error buffer for anet.c 
    int ipfd[2];                 // 默认0:ipv4、1:ipv6 
    int ipfdCount;              // 已绑定总量 

    int statRejectedConn;
    int statNumConnections;

    dict *snodes;                   // key 是 fdstr 
    size_t snodeMaxQuerybufLen;  // Limit for client query buffer length 

    list *scriptServiceRequestCtxPool;

    int tcpkeepalive;

    ntRespSnode_t* currentRespSnode;

    dict* services;
} ntRespServer_t;

void NTResp_readQueryFromSnode(aeLooper_t *el, int fd, void *privdata, int mask);
ntRespSnode_t* NTResp_snodeArgvEmpty(ntRespSnode_t *sn);
ntRespSnode_t* NTResp_snodeArgvFree(ntRespSnode_t *sn);
void NTResp_resetSnodeArgs(ntRespSnode_t *sn);

int NTResp_Prepare(int port);
sds NTResp_CatSnodeInfoString(sds s, ntRespSnode_t *sn);
ntRespSnode_t* NTResp_ConnectSnode(char *addr, int port);
void NTResp_AddReplyError(ntRespSnode_t *sn, char *err);
void NTResp_AddReplyStringArgv(ntRespSnode_t *sn, int argc, char **argv);
void NTResp_AddReplyMultiString(ntRespSnode_t *sn, int count, ...);
void NTResp_AddReplyMultiSds(ntRespSnode_t *sn, int count, ...);
void NTResp_AddReplySds(ntRespSnode_t *sn, sds data);
void NTResp_AddReplyRawSds(ntRespSnode_t *sn, sds data);
void NTResp_AddReplyString(ntRespSnode_t *sn, char *data);
void NTResp_AddReplyRawString(ntRespSnode_t *sn, char *data);
ntScriptServiceRequestCtx_t* NTResp_NewScriptServiceRequestCtx();
void NTResp_RecycleScriptServiceRequestCtx(void *_ctx);
void NTResp_FreeSnode(ntRespSnode_t *sn);  // dangerous 
ntRespSnode_t* NTResp_GetSnodeByFDS(const char *fdstr);

#endif
