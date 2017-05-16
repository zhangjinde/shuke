//
// Created by Yu Yang <yyangplus@NOSPAM.gmail.com> on 2017-05-05
//

#ifndef _SHUKE_H_
#define _SHUKE_H_ 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "defines.h"
#include "version.h"
#include "ae.h"
#include "anet.h"
#include "zmalloc.h"
#include "endianconv.h"
#include "list.h"
#include "log.h"
#include "ds.h"

#define TIME_INTERVAL 1000

#define TASK_RELOAD_ZONE     1
#define TASK_RELOAD_ALL      2

#define CONN_READ_N     0     /**< reading in a fixed number of bytes */
#define CONN_READ_LEN   1     /**< reading length bytes */
#define CONN_SWALLOW    2     /**< swallowing unnecessary bytes w/o storing */
#define CONN_WRITE_N    3     /**< writing out fixed number of bytes */
#define CONN_WRITE_LEN  4     /**< writing out the length bytes */
#define CONN_CLOSING    5     /**< closing this connection */
#define CONN_CLOSED     6     /**< connection is closed */
#define CONN_MAX_STATE  7     /**< Max state value (used for assertion) */

#define TQLock() rte_spinlock_lock(&(sk.tq_lock))
#define TQUnlock() rte_spinlock_unlock(&(sk.tq_lock))
#define TQInitLock() rte_spinlock_init(&(sk.tq_lock))

enum taskStates {
    TASK_PENDING = 0,
    TASK_RUNNING = 1,
    TASK_ERROR = 2,
    TASK_FINISHED = 3,
};

typedef struct {
    int type;
} object;

typedef struct {
    int type;

    int status;

    char *dotOrigin;  // origin in <label dot> format
    uint32_t sn;      // serial number in current cache.
    // last reload timestamp, if it is -1 then dotOrigin is a new zone.
    long ts;

    RRParser *psr;
    size_t nr_names;  // number of pending names.
    zone *new_zn;
}zoneReloadTask;

struct sk_stat {
    unsigned long long nr_req;                   // number of processed requests
    unsigned long long nr_input_bytes;
    unsigned long long nr_output_bytes;
    unsigned long long total_tcp_conn;           // total number of tcp connection received
};

struct shuke {
    char errstr[ERR_STR_LEN];

    // config
    char *configfile;

    char *coremask;
    char *mem_channels;
    int portmask;
    bool promiscuous_on;
    bool numa_on;
    bool jumbo_on;
    bool parse_ptype;
    int max_pkt_len;
    char *rx_queue_config;

    int port;
    char *pidfile;
    bool daemonize;
    char *query_log_file;
    char *logLevelStr;
    char *logfile;
    bool logVerbose;

    int tcp_backlog;
    int tcp_keepalive;
    int tcp_idle_timeout;
    int max_tcp_connections;

    char *zone_files_root;
    dict *zone_files_dict;

    char *redis_host;
    int redis_port;
    char *redis_zone_prefix;
    char *redis_soa_prefix;
    char *redis_origins_key;
    long redis_retry_interval;

    char *mongo_host;
    int mongo_port;
    char *admin_host;
    int admin_port;

    char *data_store;
    int all_reload_interval;
    bool minimize_resp;
    // end config

    struct sk_stat stat;

    volatile bool force_quit;
    zoneDict *zd;
    FILE *query_log_fp;

    int (*syncGetAllZone)(void);
    int (*initAsyncContext)(void);
    int (*checkAsyncContext)(void);
    /*
     * these two functions should be called only in mainThreadCron.
     *
     * if zone reloading is needed, just enqueue an zoneReloadTask to task queue.
     * if all zone reloading is needed, just set `last_all_reload_ts` to `now-all_reload_interval`
     * then mainThreadCron will do reloading automatically.
     */
    int (*asyncReloadAllZone)(void);
    int (*asyncReloadZone)(zoneReloadTask *t);

    // admin server
    int fd;
    dict *commands;
    struct list_head head;


    int arch_bits;
    long last_all_reload_ts; // timestamp of last all reload


    aeEventLoop *el;      // event loop for main thread.

    rte_spinlock_t tq_lock;
    struct rte_ring *tq;            // task queue, used for async tasks
    dict *tq_origins;     // the origins which is reloading.

    // redis context
    // it will be NULL when cdns is disconnected with redis,

    // redisAsyncContext *redis_ctx;
    long last_redis_retry_ts;

    bool stop_asap;

    time_t unixtime;
    long long mstime;

    time_t    starttime;     // server start time
    long long zone_load_time;

};
/*----------------------------------------------
 *     Extern declarations
 *---------------------------------------------*/
extern struct shuke sk;
extern dictType commandTableDictType;
extern dictType zoneFileDictType;

int snpack(char *buf, int offset, size_t size, char const *fmt, ...);
/*----------------------------------------------
 *     zoneReloadTask
 *---------------------------------------------*/
zoneReloadTask *zoneReloadTaskCreate(char *dotOrigin, uint32_t sn, long ts);
void zoneReloadTaskReset(zoneReloadTask *t);
void zoneReloadTaskDestroy(zoneReloadTask *t);

int enqueueZoneReloadTaskRaw(char *dotOrigin, uint32_t sn, long ts);
int enqueueZoneReloadTask(zoneReloadTask *t);
void *dequeueTask(void);
/*----------------------------------------------
 *     admin server
 *---------------------------------------------*/
int initAdminServer(void);
void releaseAdminServer(void);

/*----------------------------------------------
 *     mongo
 *---------------------------------------------*/
int initMongo(void);
int checkMongo(void);
int mongoGetAllZone(void);
int mongoAsyncReloadZone(zoneReloadTask *t);
int mongoAsyncReloadAllZone(void);

int processUDPDnsQuery(char *buf, size_t sz, char *resp, size_t respLen,
                       char *src_addr, uint16_t src_port, bool is_ipv4);
#endif /* _SHUKE_H_ */
