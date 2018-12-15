/*
 * server.h - Each SiriDB database has at least one server.
 */
#ifndef SIRIDB_SERVER_H_
#define SIRIDB_SERVER_H_

#define FLAG_KEEP_PKG 1
#define FLAG_ONLY_CHECK_ONLINE 2

#define SERVER_FLAG_RUNNING 1
#define SERVER_FLAG_SYNCHRONIZING 2
#define SERVER_FLAG_REINDEXING 4
#define SERVER_FLAG_BACKUP_MODE 8
#define SERVER_FLAG_QUEUE_FULL 16       /* never set on 'this' server */
#define SERVER_FLAG_UNAVAILABLE 32      /* never set on 'this' server */
#define SERVER_FLAG_AUTHENTICATED 64    /* must be the last (we depend on this)
                                           and will NEVER be set on 'this'
                                           server */

/* RUNNING + AUTHENTICATED                      */
#define SERVER__IS_ONLINE 65

/* RUNNING + SYNCHRONIZING + AUTHENTICATED      */
#define SERVER__IS_SYNCHRONIZING 67

/* RUNNING + REINDEXING + AUTHENTICATED         */
#define SERVER__IS_REINDEXING 69

#define SERVER__SELF_ONLINE 1           /* RUNNING                      */
#define SERVER__SELF_SYNCHRONIZING 3    /* RUNNING + SYNCHRONIZING      */
#define SERVER__SELF_REINDEXING 5       /* RUNNING + REINDEXING         */


/*
 * Server is 'connected' when at least connected.
 */
#define siridb_server_is_connected(server) \
    (server->socket != NULL)

/*
 * Server is 'online' when at least running and authenticated but not
 * queue-full. (unavailable status is intentionally ignored)
 */
#define siridb_server_is_online(server) \
((server->flags & SERVER__IS_ONLINE) == SERVER__IS_ONLINE && \
        (~server->flags & SERVER_FLAG_QUEUE_FULL))
#define siridb_server_self_online(server) \
((server->flags & SERVER__SELF_ONLINE) == SERVER__SELF_ONLINE)

/*
 * Server is 'available' when exactly running and authenticated.
 */
#define siridb_server_is_available(server) \
(server->flags == SERVER__IS_ONLINE)
#define siridb_server_self_available(server) \
(server->flags == SERVER__SELF_ONLINE)

/*
 * Server is 'synchronizing' when exactly running, authenticated and
 * synchronizing.
 */
#define siridb_server_is_synchronizing(server) \
(server->flags == SERVER__IS_SYNCHRONIZING)
#define siridb_server_self_synchronizing(server) \
(server->flags == SERVER__SELF_SYNCHRONIZING)

/*
 * Server is 'accessible' when exactly running, authenticated and optionally
 * re-indexing.
 */
#define siridb_server_is_accessible(server) \
(server->flags == SERVER__IS_ONLINE || server->flags == SERVER__IS_REINDEXING)
#define siridb_server_self_accessible(server) \
(server->flags == SERVER__SELF_ONLINE || server->flags == SERVER__SELF_REINDEXING)

typedef struct siridb_server_s siridb_server_t;
typedef struct siridb_server_walker_s siridb_server_walker_t;
typedef struct siridb_server_async_s siridb_server_async_t;

#include <uuid/uuid.h>
#include <stdint.h>
#include <siri/db/db.h>
#include <imap/imap.h>
#include <cexpr/cexpr.h>
#include <uv.h>
#include <siri/net/promise.h>
#include <siri/net/pkg.h>
#include <siri/net/stream.h>

siridb_server_t * siridb_server_new(
        const char * uuid,
        const char * address,
        size_t address_len,
        uint16_t port,
        uint16_t pool);



void siridb_server_connect(siridb_t * siridb, siridb_server_t * server);
int siridb_server_send_pkg(
        siridb_server_t * server,
        sirinet_pkg_t * pkg,
        uint64_t timeout,
        sirinet_promise_cb cb,
        void * data,
        int flags);
void siridb_server_send_flags(siridb_server_t * server);
int siridb_server_update_address(
        siridb_t * siridb,
        siridb_server_t * server,
        const char * address,
        uint16_t port);
char * siridb_server_str_status(siridb_server_t * server);
siridb_server_t * siridb_server_from_node(
        siridb_t * siridb,
        cleri_node_t * server_node,
        char * err_msg);
int siridb_server_drop(siridb_t * siridb, siridb_server_t * server);
int siridb_server_is_remote_prop(uint32_t prop);
int siridb_server_cexpr_cb(
        siridb_server_walker_t * wserver,
        cexpr_condition_t * cond);
siridb_server_t * siridb_server_register(
        siridb_t * siridb,
        unsigned char * data,
        size_t len);
void siridb__server_free(siridb_server_t * server);

/* This will remove the unavailable status but the authenticated and queue_full
 * flags are kept.
 */
#define siridb_server_update_flags(org, new) \
    org = new | (org & (SERVER_FLAG_AUTHENTICATED | SERVER_FLAG_QUEUE_FULL))

#define siridb_server_incref(server) server->ref++

/*
 * Decrement server reference counter and free the server when zero is reached.
 * When the server is destroyed, all remaining server->promises are cancelled
 * and each promise->cb() will be called.
 */
#define siridb_server_decref(server__) \
    if (!--server__->ref) siridb__server_free(server__)

struct siridb_server_s
{
    uint16_t ref;  /* keep ref on top */
    uint16_t port;
    uint16_t pool;
    uint8_t flags; /* do not use flags above 16384 */
    uint8_t id; /* set when added to a pool to either 0 or 1 */
    char * name; /* this is a format for address:port but we use it a lot */
    char * address;
    imap_t * promises;
    sirinet_stream_t * client;
    uint16_t pid;
    /* fixed server properties */
    uint8_t ip_support;
    uint8_t pad0;
    uint32_t startup_time;
    char * libuv;
    char * version;
    char * dbpath;
    char * buffer_path;
    size_t buffer_size;
    uuid_t uuid;
};

struct siridb_server_walker_s
{
    siridb_server_t * server;
    siridb_t * siridb;
};

struct siridb_server_async_s
{
    uint16_t pid;
    sirinet_stream_t * client;
};

/*
 * Returns < 0 if the uuid from server A is less than uuid from server B.
 * Returns > 0 if the uuid from server A is greater than uuid from server B.
 * Returns 0 when uuid server A and B are equal.
 */
static inline int siridb_server_cmp(siridb_server_t * sa, siridb_server_t * sb)
{
    return uuid_compare(sa->uuid, sb->uuid);
}

#endif  /* SIRIDB_SERVER_H_ */
