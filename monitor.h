/*
 * monitor.h - Module de surveillance base sur inotify.
 *
 * Initialise un descripteur inotify, ajoute un watch sur le repertoire
 * cible et expose une boucle d'ecoute (fonction monitor_run) qui pousse
 * chaque evenement traduit dans la file partagee. Cette fonction est
 * concue pour s'executer dans un thread dedie ; elle s'arrete des que le
 * drapeau global d'arret est positionne ou que la file est fermee.
 */

#ifndef SECUREBOX_MONITOR_H
#define SECUREBOX_MONITOR_H

#include "utils.h"

typedef struct {
    char           target_dir[MAX_PATH_LEN];
    int            inotify_fd;
    int            watch_fd;
    event_queue_t *queue;
} monitor_ctx_t;

int  monitor_init(monitor_ctx_t *ctx, const char *target_dir,
                  event_queue_t *queue);
void monitor_close(monitor_ctx_t *ctx);

/* Boucle d'ecoute - destinee a etre passee a pthread_create.
 * arg pointe sur un monitor_ctx_t deja initialise. */
void *monitor_run(void *arg);

#endif /* SECUREBOX_MONITOR_H */
