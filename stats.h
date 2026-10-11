/*
 * stats.h - Compteurs d'activite thread-safe et resume final.
 *
 * Maintient le nombre total d'evenements, la repartition par type et le
 * nombre d'alertes, ainsi que la duree de fonctionnement.
 */

#ifndef SECUREBOX_STATS_H
#define SECUREBOX_STATS_H

#include <pthread.h>
#include <stdio.h>
#include <time.h>

#include "config.h"

typedef struct {
    unsigned long total_events;
    unsigned long creations;
    unsigned long deletions;
    unsigned long modifications;
    unsigned long renames;
    unsigned long alerts;
    time_t        start_time;
    pthread_mutex_t mutex;
} stats_t;

int  stats_init(stats_t *s);
void stats_destroy(stats_t *s);

/* Incremente le compteur correspondant a `type`. */
void stats_record_event(stats_t *s, event_type_t type);

/* Incremente le compteur d'alertes. */
void stats_record_alert(stats_t *s);

/* Imprime un resume lisible sur le flux `stream` (typiquement stdout). */
void stats_print(stats_t *s, FILE *stream);

#endif /* SECUREBOX_STATS_H */
