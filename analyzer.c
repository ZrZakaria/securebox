/*
 * analyzer.c - Implementation du module d'analyse.
 *
 * Le module maintient :
 *   - une fenetre glissante simple des dernieres suppressions ;
 *   - une table d'horodatages par fichier modifie pour detecter les
 *     rafales de modifications.
 *
 * L'etat interne est protege par un mutex car plusieurs threads
 * pourraient theoriquement consulter l'analyseur (extension future).
 */

#define _POSIX_C_SOURCE 200809L

#include "analyzer.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/* Fenetre glissante des suppressions : on ne conserve que celles tombant
 * dans la fenetre DELETE_WINDOW_SEC. */
typedef struct {
    time_t timestamps[DELETE_THRESHOLD * 4]; /* marge pour rafales */
    int    count;
} delete_window_t;

/* Suivi par fichier des modifications recentes. */
typedef struct {
    char   path[MAX_PATH_LEN];
    time_t timestamps[MODIFY_THRESHOLD * 4];
    int    count;
    int    used;
} modify_track_t;

static delete_window_t g_del_window;
static modify_track_t  g_mod_track[MAX_TRACKED_FILES];
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

void analyzer_init(void)
{
    pthread_mutex_lock(&g_mutex);
    memset(&g_del_window, 0, sizeof(g_del_window));
    memset(g_mod_track, 0, sizeof(g_mod_track));
    pthread_mutex_unlock(&g_mutex);
}

void analyzer_destroy(void)
{
    /* Rien a liberer pour l'instant - la structure est statique. */
}

/* ---------- Regles individuelles ---------- */

/* Compte les suppressions survenues dans la fenetre [now - W, now]. */
static int record_and_count_deletions(time_t now)
{
    /* Compacte le tableau en supprimant les timestamps trop vieux. */
    int kept = 0;
    for (int i = 0; i < g_del_window.count; ++i) {
        if (now - g_del_window.timestamps[i] <= DELETE_WINDOW_SEC) {
            g_del_window.timestamps[kept++] = g_del_window.timestamps[i];
        }
    }
    /* Ajoute l'evenement courant si la place le permet. */
    int cap = (int)(sizeof(g_del_window.timestamps) /
                    sizeof(g_del_window.timestamps[0]));
    if (kept < cap) {
        g_del_window.timestamps[kept++] = now;
    }
    g_del_window.count = kept;
    return kept;
}

/* Cherche (ou alloue) un suivi pour le fichier `path`. Renvoie un pointeur
 * sur l'entree, ou NULL si la table est saturee. */
static modify_track_t *find_or_alloc_track(const char *path)
{
    int free_slot = -1;
    for (int i = 0; i < MAX_TRACKED_FILES; ++i) {
        if (g_mod_track[i].used &&
            strncmp(g_mod_track[i].path, path, MAX_PATH_LEN) == 0) {
            return &g_mod_track[i];
        }
        if (!g_mod_track[i].used && free_slot < 0) {
            free_slot = i;
        }
    }
    if (free_slot < 0) return NULL;
    modify_track_t *t = &g_mod_track[free_slot];
    memset(t, 0, sizeof(*t));
    t->used = 1;
    snprintf(t->path, MAX_PATH_LEN, "%s", path);
    return t;
}

static int record_and_count_modifications(const char *path, time_t now)
{
    modify_track_t *t = find_or_alloc_track(path);
    if (t == NULL) return 0;

    int kept = 0;
    int cap = (int)(sizeof(t->timestamps) / sizeof(t->timestamps[0]));
    for (int i = 0; i < t->count; ++i) {
        if (now - t->timestamps[i] <= MODIFY_WINDOW_SEC) {
            t->timestamps[kept++] = t->timestamps[i];
        }
    }
    if (kept < cap) {
        t->timestamps[kept++] = now;
    }
    t->count = kept;
    return kept;
}

static int is_executable_file(const char *path)
{
    struct stat st;
    if (lstat(path, &st) != 0) return 0;
    if (!S_ISREG(st.st_mode)) return 0;
    return (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) ? 1 : 0;
}

/* ---------- API publique ---------- */

severity_t analyzer_check(const event_t *ev,
                          char *out_alert_msg, size_t msg_size)
{
    if (out_alert_msg != NULL && msg_size > 0) {
        out_alert_msg[0] = '\0';
    }
    if (ev == NULL) return SEV_INFO;

    severity_t sev = SEV_INFO;
    const char *name = utils_basename(ev->path);
    time_t now = ev->timestamp;

    pthread_mutex_lock(&g_mutex);

    /* Regle 1 : rafale de suppressions. */
    if (ev->type == EVT_DELETE_FILE || ev->type == EVT_DELETE_DIR) {
        int n = record_and_count_deletions(now);
        if (n > DELETE_THRESHOLD) {
            sev = SEV_ALERTE;
            if (out_alert_msg != NULL) {
                snprintf(out_alert_msg, msg_size,
                    "Activite suspecte detectee : %d suppressions en %d secondes",
                    n, DELETE_WINDOW_SEC);
            }
        }
    }

    /* Regle 2 : modifications rapides repetees sur un meme fichier. */
    if (sev == SEV_INFO && ev->type == EVT_MODIFY_FILE) {
        int n = record_and_count_modifications(ev->path, now);
        if (n > MODIFY_THRESHOLD) {
            sev = SEV_ALERTE;
            if (out_alert_msg != NULL) {
                snprintf(out_alert_msg, msg_size,
                    "Modifications rapides repetees (%d en %d s) sur %s",
                    n, MODIFY_WINDOW_SEC, ev->path);
            }
        }
    }

    /* Regle 3 : apparition d'un fichier cache inattendu. */
    if (sev == SEV_INFO && ev->type == EVT_CREATE_FILE &&
        utils_is_hidden(name)) {
        sev = SEV_ALERTE;
        if (out_alert_msg != NULL) {
            snprintf(out_alert_msg, msg_size,
                "Creation d'un fichier cache : %s", ev->path);
        }
    }

    /* Regle 4 : creation avec extension interdite. */
    if (sev == SEV_INFO && ev->type == EVT_CREATE_FILE &&
        utils_has_forbidden_extension(name)) {
        sev = SEV_ALERTE;
        if (out_alert_msg != NULL) {
            snprintf(out_alert_msg, msg_size,
                "Extension interdite detectee : %s", ev->path);
        }
    }

    /* Regle 5 : creation/deplacement d'un executable. */
    if (sev == SEV_INFO &&
        (ev->type == EVT_CREATE_FILE || ev->type == EVT_MOVE_INTERNAL) &&
        is_executable_file(ev->path)) {
        sev = SEV_ALERTE;
        if (out_alert_msg != NULL) {
            snprintf(out_alert_msg, msg_size,
                "Apparition d'un fichier executable : %s", ev->path);
        }
    }

    pthread_mutex_unlock(&g_mutex);
    return sev;
}
