/*
 * main.c - Point d'entree de SecureBox.
 *
 * Architecture choisie : multi-threads.
 *
 *   +--------------------+     event_queue_t     +--------------------+
 *   |  thread monitor    | --------------------> |  thread analyzer   |
 *   |  (inotify + push)  |   producer/consumer   |  (regles + log)    |
 *   +--------------------+                       +--------------------+
 *               ^                                          |
 *               |  signaux (SIGINT/TERM/USR1)              v
 *               +-------- thread principal -------- stats + logs
 *
 * Le thread principal :
 *   - parse les arguments et valide le repertoire cible ;
 *   - initialise tous les modules (logger, stats, queue, analyzer, signals) ;
 *   - lance le thread monitor et le thread analyzer ;
 *   - boucle a faible frequence pour consommer les demandes SIGUSR1 ;
 *   - a l'arret, ferme la file, joint les threads, ecrit le resume final.
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "analyzer.h"
#include "config.h"
#include "logger.h"
#include "monitor.h"
#include "signals.h"
#include "stats.h"
#include "utils.h"

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage : %s <repertoire_a_surveiller>\n"
        "\n"
        "Exemple :\n"
        "  %s /home/etudiant/documents\n", prog, prog);
}

/* Contexte passe au thread d'analyse. */
typedef struct {
    event_queue_t *queue;
    stats_t       *stats;
} analyzer_args_t;

/*
 * Thread d'analyse : pop sur la file, classifie l'evenement, met a jour les
 * stats puis appelle le logger avec le bon niveau de gravite.
 */
static void *analyzer_thread(void *arg)
{
    analyzer_args_t *a = (analyzer_args_t *)arg;
    if (a == NULL) return NULL;

    event_t ev;
    char    alert_msg[MAX_MSG_LEN];

    while (queue_pop(a->queue, &ev) == 0) {
        severity_t sev = analyzer_check(&ev, alert_msg, sizeof(alert_msg));
        stats_record_event(a->stats, ev.type);
        logger_log_event(&ev, sev);
        if (sev == SEV_ALERTE) {
            stats_record_alert(a->stats);
            if (alert_msg[0] != '\0') {
                logger_log_message(SEV_ALERTE, "%s", alert_msg);
            }
        }
    }
    logger_log_message(SEV_INFO, "Thread d'analyse termine.");
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    const char *target_dir = argv[1];

    if (utils_check_directory(target_dir) != 0) {
        return EXIT_FAILURE;
    }

    /* ---------- Initialisation des modules ---------- */
    if (logger_init(LOG_FILE) != 0) {
        return EXIT_FAILURE;
    }

    stats_t stats;
    if (stats_init(&stats) != 0) {
        fprintf(stderr, "[ERREUR] stats_init\n");
        logger_close();
        return EXIT_FAILURE;
    }

    /* La file est large (chemins + buffer circulaire) : on l'alloue
     * statiquement plutot que sur la pile pour eviter tout depassement. */
    static event_queue_t queue;
    if (queue_init(&queue) != 0) {
        fprintf(stderr, "[ERREUR] queue_init\n");
        stats_destroy(&stats);
        logger_close();
        return EXIT_FAILURE;
    }

    analyzer_init();

    if (signals_install(&stats, &queue) != 0) {
        analyzer_destroy();
        queue_destroy(&queue);
        stats_destroy(&stats);
        logger_close();
        return EXIT_FAILURE;
    }

    monitor_ctx_t mon;
    if (monitor_init(&mon, target_dir, &queue) != 0) {
        analyzer_destroy();
        queue_destroy(&queue);
        stats_destroy(&stats);
        logger_close();
        return EXIT_FAILURE;
    }

    logger_log_message(SEV_INFO,
        "SecureBox demarre - cible='%s' pid=%ld",
        target_dir, (long)getpid());
    fprintf(stdout,
        "SecureBox surveille '%s'. Ctrl+C pour arreter, SIGUSR1 pour stats.\n",
        target_dir);
    fflush(stdout);

    /* ---------- Lancement des threads ---------- */
    pthread_t       th_monitor;
    pthread_t       th_analyzer;
    analyzer_args_t aargs = { .queue = &queue, .stats = &stats };

    if (pthread_create(&th_monitor, NULL, monitor_run, &mon) != 0) {
        fprintf(stderr, "[ERREUR] pthread_create monitor : %s\n",
                strerror(errno));
        monitor_close(&mon);
        analyzer_destroy();
        queue_destroy(&queue);
        stats_destroy(&stats);
        logger_close();
        return EXIT_FAILURE;
    }
    if (pthread_create(&th_analyzer, NULL, analyzer_thread, &aargs) != 0) {
        fprintf(stderr, "[ERREUR] pthread_create analyzer : %s\n",
                strerror(errno));
        queue_close(&queue);
        pthread_join(th_monitor, NULL);
        monitor_close(&mon);
        analyzer_destroy();
        queue_destroy(&queue);
        stats_destroy(&stats);
        logger_close();
        return EXIT_FAILURE;
    }

    /* ---------- Boucle principale ---------- */
    while (!signals_stop_requested()) {
        if (signals_consume_stats_request()) {
            stats_print(&stats, stdout);
            logger_log_message(SEV_INFO, "Statistiques affichees via SIGUSR1.");
        }
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 200 * 1000 * 1000 };
        nanosleep(&ts, NULL);
    }

    /* ---------- Arret propre ---------- */
    logger_log_message(SEV_INFO, "Arret demande, fermeture en cours...");

    /* Signaler au thread monitor de s'arreter et fermer la file pour reveiller
     * le thread d'analyse. */
    queue_close(&queue);
    pthread_join(th_monitor, NULL);
    pthread_join(th_analyzer, NULL);

    monitor_close(&mon);
    analyzer_destroy();
    queue_destroy(&queue);

    stats_print(&stats, stdout);
    logger_log_message(SEV_INFO,
        "SecureBox arrete - evenements=%lu alertes=%lu",
        stats.total_events, stats.alerts);

    stats_destroy(&stats);
    logger_close();
    return EXIT_SUCCESS;
}
