/*
 * queries.c - Query helpers for listener.
 */
#include <assert.h>
#include <logger/logger.h>
#include <siri/db/aggregate.h>
#include <siri/db/query.h>
#include <siri/db/shard.h>
#include <siri/db/queries.h>
#include <stddef.h>
#include <stdlib.h>

#define DEFAULT_LIST_LIMIT 1000

#define QUERIES_NEW(q)              \
q->flags = 0;                       \
q->series_map = NULL;               \
q->series_tmp = NULL;               \
q->vec = NULL;                    \
q->vec_index = 0;                 \
q->pmap = NULL;                     \
q->update_cb = NULL;                \
q->where_expr = NULL;               \
q->regex = NULL;                    \
q->match_data = NULL;


#define QUERIES_FREE(q, handle)                                 \
if (q->series_map != NULL)                                      \
{                                                               \
    imap_free(                                                  \
            q->series_map,                                      \
            (imap_free_cb) &siridb__series_decref);             \
}                                                               \
if (q->series_tmp != NULL)                                      \
{                                                               \
    imap_free(                                                  \
            q->series_tmp,                                      \
            (imap_free_cb) &siridb__series_decref);             \
}                                                               \
if (q->vec != NULL)                                           \
{                                                               \
    siridb_series_t * series;                                   \
    for (; q->vec_index < q->vec->len; q->vec_index++)    \
    {                                                                   \
        series = (siridb_series_t *) q->vec->data[q->vec_index];    \
        siridb_series_decref(series);                                   \
    }                                                                   \
    vec_free(q->vec);                                       \
}                                                               \
if (q->where_expr != NULL)                                      \
{                                                               \
    cexpr_free(q->where_expr);                                  \
}                                                               \
if (q->pmap != NULL)                                            \
{                                                               \
    imap_free(q->pmap, NULL);                                   \
}                                                               \
pcre2_code_free(q->regex);                                      \
pcre2_match_data_free(q->match_data);                           \
free(q);                                                        \
siridb_query_free(handle);

static void QUERIES_free_merge_result(vec_t * plist);

query_select_t * query_select_new(void)
{
    query_select_t * q_select =
            (query_select_t *) malloc(sizeof(query_select_t));

    if (q_select == NULL)
    {
        return NULL;
    }
    QUERIES_NEW(q_select)

    q_select->tp = QUERIES_SELECT;
    q_select->start_ts = NULL;
    q_select->end_ts = NULL;
    q_select->presuf = NULL;
    q_select->merge_as = NULL;
    q_select->n = 0;
    q_select->nselects = 1;  /* we have at least one select function  */
    q_select->points_map = NULL;
    q_select->alist = NULL;
    q_select->mlist = NULL;
    q_select->result = ct_new();

    if (q_select->result == NULL)
    {
        free(q_select);
        return NULL;
    }

    return q_select;
}

query_alter_t * query_alter_new(void)
{
    query_alter_t * q_alter =
            (query_alter_t *) malloc(sizeof(query_alter_t));

    if (q_alter == NULL)
    {
        return NULL;
    }

    QUERIES_NEW(q_alter)

    q_alter->tp = QUERIES_ALTER;
    q_alter->alter_tp = QUERY_ALTER_NONE;
    q_alter->via.dummy = NULL;
    q_alter->n = 0;

    return q_alter;
}

query_count_t * query_count_new(void)
{
    query_count_t * q_count =
            (query_count_t *) malloc(sizeof(query_count_t));

    if (q_count == NULL)
    {
        return NULL;
    }

    QUERIES_NEW(q_count)

    q_count->tp = QUERIES_COUNT;
    q_count->n = 0;

    return q_count;
}

query_drop_t * query_drop_new(void)
{
    query_drop_t * q_drop =
            (query_drop_t *) malloc(sizeof(query_drop_t));

    if (q_drop == NULL)
    {
        return NULL;
    }

    QUERIES_NEW(q_drop)

    q_drop->tp = QUERIES_DROP;
    q_drop->n = 0;
    q_drop->flags = 0;
    q_drop->shards_list = NULL;

    return q_drop;
}

query_list_t * query_list_new(void)
{
    query_list_t * q_list =
            (query_list_t *) malloc(sizeof(query_list_t));

    if (q_list == NULL)
    {
        return NULL;
    }

    QUERIES_NEW(q_list)

    q_list->tp = QUERIES_LIST;
    q_list->props = NULL;
    q_list->limit = DEFAULT_LIST_LIMIT;

    return q_list;
}

void query_alter_free(uv_handle_t * handle)
{
    query_alter_t * q_alter =
            (query_alter_t *) ((siridb_query_t *) handle->data)->data;

    switch (q_alter->alter_tp)
    {
    case QUERY_ALTER_NONE:
    case QUERY_ALTER_DATABASE:
    case QUERY_ALTER_SERVERS:
        break;
    case QUERY_ALTER_GROUP:
        siridb_group_decref(q_alter->via.group);
        break;
    case QUERY_ALTER_SERVER:
        siridb_server_decref(q_alter->via.server);
        break;
    case QUERY_ALTER_USER:
        siridb_user_decref(q_alter->via.user);
        break;
    default:
        assert(0);
    }

    QUERIES_FREE(q_alter, handle)
}

void query_count_free(uv_handle_t * handle)
{
    query_count_t * q_count =
            (query_count_t *) ((siridb_query_t *) handle->data)->data;

    QUERIES_FREE(q_count, handle)
}

void query_drop_free(uv_handle_t * handle)
{
    query_drop_t * q_drop =
            (query_drop_t *) ((siridb_query_t *) handle->data)->data;

    if (q_drop->shards_list != NULL)
    {
        siridb_shard_t * shard;
        while (q_drop->shards_list->len)
        {
            shard = (siridb_shard_t *) vec_pop(q_drop->shards_list);
            siridb_shard_decref(shard);
        }

        vec_free(q_drop->shards_list);
    }

    QUERIES_FREE(q_drop, handle)
}

void query_list_free(uv_handle_t * handle)
{
    query_list_t * q_list =
            (query_list_t *) ((siridb_query_t *) handle->data)->data;

    if (q_list->props != NULL)
    {
        vec_free(q_list->props);
    }

    QUERIES_FREE(q_list, handle)
}

void query_select_free(uv_handle_t * handle)
{
    query_select_t * q_select =
            (query_select_t *) ((siridb_query_t *) handle->data)->data;

    siridb_presuf_free(q_select->presuf);

    if (q_select->points_map != NULL)
    {
        imap_free(q_select->points_map, (imap_free_cb) &siridb_points_free);
    }

    if (q_select->result != NULL)
    {
        if (q_select->merge_as == NULL)
        {
            ct_free(q_select->result, (ct_free_cb) &siridb_points_free);
        }
        else
        {
            ct_free(q_select->result, (ct_free_cb) &QUERIES_free_merge_result);
        }
    }

    free(q_select->merge_as);

    if (q_select->alist != NULL)
    {
        siridb_aggregate_list_free(q_select->alist);
    }

    if (q_select->mlist != NULL)
    {
        siridb_aggregate_list_free(q_select->mlist);
    }

    QUERIES_FREE(q_select, handle)
}

void query_help_free(uv_handle_t * handle)
{
    /* used as char to hold a string */
    free(((siridb_query_t *) handle->data)->data);

    /* normal call-back */
    siridb_query_free(handle);
}

static void QUERIES_free_merge_result(vec_t * plist)
{
    size_t i;
    for (i = 0; i < plist->len; i ++)
    {
        siridb_points_free(plist->data[i]);
    }
    free(plist);
}
