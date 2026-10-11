/*
 * signals.c - Implementation de la gestion des signaux.
 */

#define _POSIX_C_SOURCE 200809L

#include "signals.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

static volatile sig_atomic_t g_stop_requested  = 0;
static volatile sig_atomic_t g_stats_requested = 0;

static stats_t        *g_stats = NULL;
static event_queue_t  *g_queue = NULL;

static void on_stop(int signo)
{
    (void)signo;
    g_stop_requested = 1;
    /* Reveiller toutes les threads bloquees dans la file. */
    if (g_queue != NULL) {
        queue_close(g_queue);
    }
}

static void on_stats(int signo)
{
    (void)signo;
    g_stats_requested = 1;
}

int signals_install(stats_t *stats, event_queue_t *queue)
{
    g_stats = stats;
    g_queue = queue;

    struct sigaction sa_stop;
    memset(&sa_stop, 0, sizeof(sa_stop));
    sa_stop.sa_handler = on_stop;
    sigemptyset(&sa_stop.sa_mask);
    sa_stop.sa_flags = 0; /* pas de SA_RESTART : on veut que poll() rende EINTR */

    if (sigaction(SIGINT,  &sa_stop, NULL) != 0 ||
        sigaction(SIGTERM, &sa_stop, NULL) != 0) {
        fprintf(stderr, "[ERREUR] sigaction stop : %s\n", strerror(errno));
        return -1;
    }

    struct sigaction sa_stats;
    memset(&sa_stats, 0, sizeof(sa_stats));
    sa_stats.sa_handler = on_stats;
    sigemptyset(&sa_stats.sa_mask);
    sa_stats.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa_stats, NULL) != 0) {
        fprintf(stderr, "[ERREUR] sigaction SIGUSR1 : %s\n", strerror(errno));
        return -1;
    }

    /* Ignorer SIGPIPE : nous gerons les ecritures via fprintf/fwrite et ne
     * voulons pas etre interrompus par un terminal ferme. */
    signal(SIGPIPE, SIG_IGN);
    return 0;
}

int signals_stop_requested(void)
{
    return (int)g_stop_requested;
}

int signals_consume_stats_request(void)
{
    if (g_stats_requested) {
        g_stats_requested = 0;
        return 1;
    }
    return 0;
}
