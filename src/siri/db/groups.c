/*
 * groups.c - Groups (saved regular expressions).
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 16-08-2016
 *
 */
#define _GNU_SOURCE
#include <siri/db/groups.h>
#include <siri/db/group.h>
#include <siri/err.h>
#include <stdlib.h>
#include <slist/slist.h>
#include <siri/db/misc.h>
#include <siri/db/db.h>
#include <unistd.h>
#include <siri/siri.h>
#include <assert.h>
#include <logger/logger.h>
#include <xpath/xpath.h>

#define SIRIDB_GROUPS_SCHEMA 1
#define SIRIDB_GROUPS_FN "groups.dat"
#define GROUPS_LOOP_SLEEP 2

static int GROUPS_load(siridb_groups_t * groups);
static void GROUPS_free(siridb_groups_t * groups);
static void GROUPS_loop(uv_work_t * work);
static void GROUPS_loop_finish(uv_work_t * work, int status);
static int GROUPS_write(siridb_group_t * group, qp_fpacker_t * fpacker);

/*
 * In case of an error the return value is NULL and a SIGNAL is raised.
 */
siridb_groups_t * siridb_groups_new(siridb_t * siridb)
{
    log_info("Loading groups");

    siridb_groups_t * groups =
            (siridb_groups_t *) malloc(sizeof(siridb_groups_t));
    if (groups == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        groups->fn = NULL;
        groups->groups = ct_new();
        groups->nseries = slist_new(SLIST_DEFAULT_SIZE);
        groups->ngroups = slist_new(SLIST_DEFAULT_SIZE);
        groups->work.data = (siridb_t *) siridb;

        if (groups->groups == NULL || groups->nseries == NULL)
        {
            GROUPS_free(groups);
            groups = NULL;  /* signal is raised */
        }
        else if (asprintf(
                    &groups->fn,
                    "%s%s",
                    siridb->dbpath,
                    SIRIDB_GROUPS_FN) < 0 || GROUPS_load(groups))
        {
            ERR_ALLOC
            GROUPS_free(groups);
            groups = NULL;
        }
        else
        {
            groups->status = GROUPS_INIT;
            groups->flags = 0;
            uv_queue_work(
                    siri.loop,
                    &groups->work,
                    GROUPS_loop,
                    GROUPS_loop_finish);
        }
    }

    return groups;
}

/*
 * Returns 0 if successful or -1 in case of an error.
 */
int siridb_groups_add_series(
        siridb_groups_t * groups,
        siridb_series_t * series)
{
    if (slist_append_safe(&groups->nseries, series))
    {
        return -1;
    }

    siridb_series_incref(series);
    groups->flags |= GROUPS_FLAG_NEW_SERIES;

    return 0;
}

int siridb_groups_add_group(
        siridb_groups_t * groups,
        const char * name,
        const char * source,
        size_t source_len,
        char * err_msg)
{
    int rc;

    siridb_group_t * group = siridb_group_new(
            name,
            source,
            source_len,
            err_msg);

    if (group == NULL)
    {
        return -1;  /* err_msg is set and a SIGNAL is possibly raised */
    }

    rc = ct_add(groups->groups, name, group);

    switch (rc)
    {
    case CT_EXISTS:
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Group '%s' already exists.",
                name);
        break;

    case CT_ERR:
        sprintf(err_msg, "Memory allocation error.");
        break;

    case CT_OK:
        if (slist_append_safe(&groups->ngroups, group))
        {
            sprintf(err_msg, "Memory allocation error.");
            rc = -1;
        }
        else
        {
            siridb_group_incref(group);
            groups->flags |= GROUPS_FLAG_NEW_GROUP;
        }
        break;

    default:
        assert (0);
        break;
    }

    if (rc != CT_OK)
    {
        siridb_group_decref(group);
    }

    return rc;
}

inline void siridb_groups_destroy(siridb_groups_t * groups)
{
    groups->status = GROUPS_STOPPING;
}

static void GROUPS_free(siridb_groups_t * groups)
{
#ifdef DEBUG
    log_debug("Free groups");
#endif
    free(groups->fn);

    if (groups->nseries != NULL)
    {
        for (size_t i = 0; i < groups->nseries->len; i++)
        {
            siridb_series_decref((siridb_series_t *) groups->nseries->data[i]);
        }
        slist_free(groups->nseries);
    }

    if (groups->groups != NULL)
    {
        ct_free(groups->groups, (ct_free_cb) siridb_group_decref);
    }

    if (groups->ngroups != NULL)
    {
        for (size_t i = 0; i < groups->ngroups->len; i++)
        {
            siridb_group_decref((siridb_group_t *) groups->ngroups->data[i]);
        }
        slist_free(groups->ngroups);
    }
}

static void GROUPS_loop(uv_work_t * work)
{
    siridb_t * siridb = (siridb_t *) work->data;
    siridb_groups_t * groups = siridb->groups;

    while (groups->status != GROUPS_STOPPING)
    {
        sleep(GROUPS_LOOP_SLEEP);

        switch((siridb_groups_status_t) groups->status)
        {
        case GROUPS_INIT:
            break;

        case GROUPS_RUNNING:
            break;

        case GROUPS_STOPPING:
            break;

        default:
            assert (0);
            break;

        }
    }

    groups->status = GROUPS_CLOSED;
}

static void GROUPS_loop_finish(uv_work_t * work, int status)
{
    /*
     * Main Thread
     */
    siridb_t * siridb = (siridb_t *) work->data;

    /* free groups */
    GROUPS_free(siridb->groups);

    /* set groups to NULL */
    siridb->groups = NULL;
}

static int GROUPS_load(siridb_groups_t * groups)
{
    int rc = 0;

    if (!xpath_file_exist(groups->fn))
    {
        return rc; // no groups file, nothing to do
    }

    qp_unpacker_t * unpacker = siridb_misc_open_schema_file(
            SIRIDB_GROUPS_SCHEMA,
            groups->fn);

    if (unpacker == NULL)
    {
        rc = -1;
    }
    else
    {
        qp_obj_t qp_name, qp_source;
        char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];

        while ( !rc &&
                qp_is_array(qp_next(unpacker, NULL)) &&
                qp_next(unpacker, &qp_name) == QP_RAW &&
                qp_is_raw_term(&qp_name) &&
                qp_next(unpacker, &qp_source) == QP_RAW)
        {
            rc = siridb_groups_add_group(
                    groups,
                    qp_name.via.raw,
                    qp_source.via.raw,
                    qp_source.len,
                    err_msg);
        }
        qp_unpacker_ff_free(unpacker);
    }

    return rc;
}

int siridb_groups_save(siridb_groups_t * groups)
{
    qp_fpacker_t * fpacker;

    return (
        /* open a new user file */
        (fpacker = qp_open(groups->fn, "w")) == NULL ||

        /* open a new array */
        qp_fadd_type(fpacker, QP_ARRAY_OPEN) ||

        /* write the current schema */
        qp_fadd_int16(fpacker, SIRIDB_GROUPS_SCHEMA) ||

        /* we can and should skip this if we have no users to save */
        ct_values(groups->groups, (ct_val_cb) GROUPS_write, fpacker) ||

        /* close file pointer */
        qp_close(fpacker)) ? EOF : 0;
}

static int GROUPS_write(siridb_group_t * group, qp_fpacker_t * fpacker)
{
    int rc = 0;

    rc += qp_fadd_type(fpacker, QP_ARRAY2);
    rc += qp_fadd_raw(fpacker, group->name, strlen(group->name) + 1);
    rc += qp_fadd_string(fpacker, group->source);

    return rc;
}