/*
 * utils.c - Implementation de la file d'evenements et des helpers generaux.
 */

#define _POSIX_C_SOURCE 200809L

#include "utils.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

/* =====================================================================
 *  File d'evenements thread-safe (file circulaire bornee)
 * ===================================================================== */

int queue_init(event_queue_t *q)
{
    if (q == NULL) {
        return -1;
    }
    q->head   = 0;
    q->tail   = 0;
    q->count  = 0;
    q->closed = 0;

    if (pthread_mutex_init(&q->mutex, NULL) != 0)       return -1;
    if (pthread_cond_init(&q->not_empty, NULL) != 0)    return -1;
    if (pthread_cond_init(&q->not_full, NULL) != 0)     return -1;
    return 0;
}

void queue_destroy(event_queue_t *q)
{
    if (q == NULL) return;
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

int queue_push(event_queue_t *q, const event_t *ev)
{
    if (q == NULL || ev == NULL) return -1;

    pthread_mutex_lock(&q->mutex);
    while (q->count == EVENT_QUEUE_CAPACITY && !q->closed) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }
    if (q->closed) {
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }
    q->buffer[q->tail] = *ev;
    q->tail = (q->tail + 1) % EVENT_QUEUE_CAPACITY;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

int queue_pop(event_queue_t *q, event_t *out)
{
    if (q == NULL || out == NULL) return -1;

    pthread_mutex_lock(&q->mutex);
    while (q->count == 0 && !q->closed) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }
    if (q->count == 0 && q->closed) {
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }
    *out = q->buffer[q->head];
    q->head = (q->head + 1) % EVENT_QUEUE_CAPACITY;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

void queue_close(event_queue_t *q)
{
    if (q == NULL) return;
    pthread_mutex_lock(&q->mutex);
    q->closed = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
}

/* =====================================================================
 *  Helpers generaux
 * ===================================================================== */

void utils_format_timestamp(time_t t, char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0) return;
    struct tm tm_info;
    localtime_r(&t, &tm_info);
    strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", &tm_info);
}

int utils_check_directory(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        fprintf(stderr, "[ERREUR] Chemin de repertoire vide.\n");
        return -1;
    }
    struct stat st;
    if (stat(path, &st) != 0) {
        fprintf(stderr, "[ERREUR] Impossible d'acceder a '%s' : %s\n",
                path, strerror(errno));
        return -1;
    }
    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "[ERREUR] '%s' n'est pas un repertoire.\n", path);
        return -1;
    }
    if (access(path, R_OK | X_OK) != 0) {
        fprintf(stderr, "[ERREUR] Droits insuffisants sur '%s' : %s\n",
                path, strerror(errno));
        return -1;
    }
    return 0;
}

int utils_is_hidden(const char *name)
{
    if (name == NULL) return 0;
    const char *base = utils_basename(name);
    return (base[0] == '.' && base[1] != '\0' &&
            !(base[1] == '.' && base[2] == '\0'));
}

int utils_has_forbidden_extension(const char *name)
{
    if (name == NULL) return 0;
    const char *dot = strrchr(name, '.');
    if (dot == NULL || dot == name) return 0;
    for (int i = 0; i < FORBIDDEN_EXTENSIONS_COUNT; ++i) {
        if (strcasecmp(dot, FORBIDDEN_EXTENSIONS[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

const char *utils_event_type_str(event_type_t t)
{
    switch (t) {
        case EVT_CREATE_FILE:   return "CREATE_FILE";
        case EVT_DELETE_FILE:   return "DELETE_FILE";
        case EVT_MODIFY_FILE:   return "MODIFY_FILE";
        case EVT_RENAME_FILE:   return "RENAME_FILE";
        case EVT_CREATE_DIR:    return "CREATE_DIR";
        case EVT_DELETE_DIR:    return "DELETE_DIR";
        case EVT_MOVE_INTERNAL: return "MOVE_INTERNAL";
        default:                return "UNKNOWN";
    }
}

const char *utils_basename(const char *path)
{
    if (path == NULL) return "";
    const char *slash = strrchr(path, '/');
    return (slash != NULL) ? slash + 1 : path;
}
