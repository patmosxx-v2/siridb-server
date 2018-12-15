/*
 * auth.c - Handle SiriDB authentication.
 */
#include <assert.h>
#include <logger/logger.h>
#include <siri/db/auth.h>
#include <siri/db/db.h>
#include <siri/db/servers.h>
#include <siri/db/users.h>
#include <siri/net/protocol.h>
#include <siri/net/stream.h>
#include <siri/siri.h>
#include <siri/version.h>
#include <stdlib.h>
#include <string.h>

cproto_server_t siridb_auth_user_request(
        sirinet_stream_t * client,
        qp_obj_t * qp_username,
        qp_obj_t * qp_password,
        qp_obj_t * qp_dbname)
{
    siridb_t * siridb;
    siridb_user_t * user;

    char dbname[qp_dbname->len + 1];
    memcpy(dbname, qp_dbname->via.raw, qp_dbname->len);
    dbname[qp_dbname->len] = 0;

    char username[qp_username->len + 1];
    memcpy(username, qp_username->via.raw, qp_username->len);
    username[qp_username->len] = 0;

    char password[qp_password->len + 1];
    memcpy(password, qp_password->via.raw, qp_password->len);
    password[qp_password->len] = 0;

    if ((siridb = siridb_get(siri.siridb_list, dbname)) == NULL)
    {
        log_warning("User authentication request failed: unknown database");
        return CPROTO_ERR_AUTH_UNKNOWN_DB;
    }

    if ((user = siridb_users_get_user(
            siridb,
            username,
            password)) == NULL)
    {
        log_warning("User authentication request failed: invalid credentials");
        return CPROTO_ERR_AUTH_CREDENTIALS;
    }

    client->siridb = siridb;
    client->origin = user;

    siridb_user_incref(user);

    return CPROTO_RES_AUTH_SUCCESS;
}

/*
 * Note: qp_version, qp_dbname, qp_min_version must by or type raw and must be
 * null terminated.
 */
bproto_server_t siridb_auth_server_request(
        sirinet_stream_t * client,
        qp_obj_t * qp_uuid,
        qp_obj_t * qp_dbname,
        qp_obj_t * qp_version,
        qp_obj_t * qp_min_version)
{
    siridb_t * siridb;
    siridb_server_t * server;
    uuid_t uuid;

    if (qp_uuid->len != 16)
    {
        return BPROTO_AUTH_ERR_INVALID_UUID;
    }

    if (siri_version_cmp(
            (const char *) qp_version->via.raw, SIRIDB_MINIMAL_VERSION) < 0)
    {
        return BPROTO_AUTH_ERR_VERSION_TOO_OLD;
    }

    if (siri_version_cmp(
            (const char *) qp_min_version->via.raw, SIRIDB_VERSION) > 0)
    {
        return BPROTO_AUTH_ERR_VERSION_TOO_NEW;
    }

    memcpy(uuid, qp_uuid->via.raw, 16);

    if ((siridb = siridb_get(
            siri.siridb_list,
            (const char *) qp_dbname->via.raw)) == NULL)
    {
        return BPROTO_AUTH_ERR_UNKNOWN_DBNAME;
    }

    if (    (server = siridb_servers_by_uuid(siridb->servers, uuid)) == NULL ||
            server == siridb->server)
    {
        /*
         * Respond with unknown uuid when not found or in case its 'this'
         * server.
         */
        return BPROTO_AUTH_ERR_UNKNOWN_UUID;
    }

    client->siridb = siridb;
    client->origin = server;

    free(server->version);
    server->version = strdup((const char *) qp_version->via.raw);

    /* we must increment the server reference counter */
    siridb_server_incref(server);

    return BPROTO_AUTH_SUCCESS;
}
