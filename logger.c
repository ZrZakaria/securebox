/*
 * logger.c - Implementation du module de journalisation.
 *
 * Le fichier est ouvert avec O_CREAT | O_WRONLY | O_APPEND via open() puis
 * encapsule dans un FILE* pour faciliter le formatage. Toutes les ecritures
 * sont serialisees par un mutex global. fsync est appele apres flush pour
 * garantir la persistance lors d'un arret signale.
 */

#define _POSIX_C_SOURCE 200809L

#include "logger.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static FILE           *g_log_fp    = NULL;
static int             g_log_fd    = -1;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char *severity_str(severity_t s)
{
    return (s == SEV_ALERTE) ? "ALERTE" : "INFO";
}

/* Cree recursivement le repertoire parent du fichier log si necessaire. */
static int ensure_parent_dir(const char *path)
{
    char tmp[MAX_PATH_LEN];
    snprintf(tmp, sizeof(tmp), "%s", path);
    char *slash = strrchr(tmp, '/');
    if (slash == NULL) return 0;
    *slash = '\0';
    if (tmp[0] == '\0') return 0;

    struct stat st;
    if (stat(tmp, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    /* Creation simple (un seul niveau attendu : ./logs). */
    if (mkdir(tmp, 0750) != 0 && errno != EEXIST) {
        fprintf(stderr, "[ERREUR] Impossible de creer '%s' : %s\n",
                tmp, strerror(errno));
        return -1;
    }
    return 0;
}

int logger_init(const char *path)
{
    if (path == NULL) return -1;
    if (ensure_parent_dir(path) != 0) return -1;

    /* O_APPEND garantit l'atomicite des ecritures concurrentes au niveau
     * du noyau. Le mode 0640 limite l'acces au proprietaire et au groupe. */
    g_log_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0640);
    if (g_log_fd < 0) {
        fprintf(stderr, "[ERREUR] Ouverture du log '%s' : %s\n",
                path, strerror(errno));
        return -1;
    }
    g_log_fp = fdopen(g_log_fd, "a");
    if (g_log_fp == NULL) {
        fprintf(stderr, "[ERREUR] fdopen log : %s\n", strerror(errno));
        close(g_log_fd);
        g_log_fd = -1;
        return -1;
    }
    /* Bufferisation ligne par ligne. */
    setvbuf(g_log_fp, NULL, _IOLBF, 0);
    return 0;
}

void logger_close(void)
{
    pthread_mutex_lock(&g_log_mutex);
    if (g_log_fp != NULL) {
        fflush(g_log_fp);
        if (g_log_fd >= 0) {
            fsync(g_log_fd);
        }
        fclose(g_log_fp);
        g_log_fp = NULL;
        g_log_fd = -1;
    }
    pthread_mutex_unlock(&g_log_mutex);
}

static void write_line(severity_t sev, const char *body)
{
    char ts[32];
    utils_format_timestamp(time(NULL), ts, sizeof(ts));

    pthread_mutex_lock(&g_log_mutex);
    if (g_log_fp != NULL) {
        fprintf(g_log_fp, "[%s] [%s] %s\n", ts, severity_str(sev), body);
        fflush(g_log_fp);
        if (g_log_fd >= 0) fsync(g_log_fd);
    }
    /* Echo console : alertes sur stderr, infos sur stdout. */
    FILE *console = (sev == SEV_ALERTE) ? stderr : stdout;
    fprintf(console, "[%s] [%s] %s\n", ts, severity_str(sev), body);
    fflush(console);
    pthread_mutex_unlock(&g_log_mutex);
}

void logger_log_event(const event_t *ev, severity_t sev)
{
    if (ev == NULL) return;
    /* Le corps doit pouvoir contenir le type + deux chemins (renommage). */
    char body[2 * MAX_PATH_LEN + 64];
    if (ev->type == EVT_RENAME_FILE && ev->old_path[0] != '\0') {
        snprintf(body, sizeof(body), "%s : %s -> %s",
                utils_event_type_str(ev->type), ev->old_path, ev->path);
    } else {
        snprintf(body, sizeof(body), "%s : %s",
                utils_event_type_str(ev->type), ev->path);
    }
    write_line(sev, body);
}

void logger_log_message(severity_t sev, const char *fmt, ...)
{
    if (fmt == NULL) return;
    char body[MAX_MSG_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);
    write_line(sev, body);
}
