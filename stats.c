/*
 * stats.c - Implementation des compteurs proteges par mutex.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "stats.h"

int stats_init(stats_t *s)
{
    if (s == NULL) return -1;
    memset(s, 0, sizeof(*s));
    s->start_time = time(NULL);
    if (pthread_mutex_init(&s->mutex, NULL) != 0) return -1;
    return 0;
}

void stats_destroy(stats_t *s)
{
    if (s == NULL) return;
    pthread_mutex_destroy(&s->mutex);
}

void stats_record_event(stats_t *s, event_type_t type)
{
    if (s == NULL) return;
    pthread_mutex_lock(&s->mutex);
    s->total_events++;
    switch (type) {
        case EVT_CREATE_FILE:
        case EVT_CREATE_DIR:
            s->creations++;
            break;
        case EVT_DELETE_FILE:
        case EVT_DELETE_DIR:
            s->deletions++;
            break;
        case EVT_MODIFY_FILE:
            s->modifications++;
            break;
        case EVT_RENAME_FILE:
        case EVT_MOVE_INTERNAL:
            s->renames++;
            break;
        default:
            break;
    }
    pthread_mutex_unlock(&s->mutex);
}

void stats_record_alert(stats_t *s)
{
    if (s == NULL) return;
    pthread_mutex_lock(&s->mutex);
    s->alerts++;
    pthread_mutex_unlock(&s->mutex);
}

void stats_print(stats_t *s, FILE *stream)
{
    if (s == NULL || stream == NULL) return;
    pthread_mutex_lock(&s->mutex);
    time_t now      = time(NULL);
    long   uptime_s = (long)(now - s->start_time);
    long   hours    = uptime_s / 3600;
    long   minutes  = (uptime_s % 3600) / 60;
    long   seconds  = uptime_s % 60;

    fprintf(stream,
        "\n========== STATISTIQUES SECUREBOX ==========\n"
        " Evenements totaux       : %lu\n"
        " Creations               : %lu\n"
        " Suppressions            : %lu\n"
        " Modifications           : %lu\n"
        " Renommages / deplacements: %lu\n"
        " Alertes generees        : %lu\n"
        " Duree de fonctionnement : %ldh %02ldm %02lds\n"
        "============================================\n",
        s->total_events, s->creations, s->deletions, s->modifications,
        s->renames, s->alerts, hours, minutes, seconds);
    fflush(stream);
    pthread_mutex_unlock(&s->mutex);
}
