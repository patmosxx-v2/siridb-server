/*
 * points.c - Array object for points.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 04-04-2016
 *
 */
#include <siri/db/points.h>
#include <logger/logger.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

siridb_points_t * siridb_new_points(size_t size)
{
    siridb_points_t * points =
            (siridb_points_t *) malloc(sizeof(siridb_points_t));
    points->len = 0;
    points->size = size;
    points->data = (siridb_point_t *) malloc(sizeof(siridb_point_t) * size);
    return points;
}

void siridb_free_points(siridb_points_t * points)
{
    free(points->data);
    free(points);
}

void siridb_points_add_point(
        siridb_points_t * points,
        uint64_t * ts,
        qp_via_t * val)
{
    size_t i;
    siridb_point_t * point;

    assert (points->len < points->size);

    for (i = points->len; i-- > 0 && (points->data + i)->ts > *ts;)
        *(points->data + i + 1) = *(points->data + i);

    points->len++;

    point = points->data + i + 1;

    (*point).ts = *ts;
    (*point).val = *val;
}
