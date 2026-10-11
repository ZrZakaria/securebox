/*
 * utils.h - Utilitaires partages et file d'evenements thread-safe.
 *
 * Definit la structure event_t echangee entre le thread de surveillance et
 * le thread d'analyse, ainsi que les fonctions utilitaires generales
 * (horodatage, verification de chemin, detection de fichier cache,
 *  detection d'extension interdite, conversion type -> chaine).
 */

#ifndef SECUREBOX_UTILS_H
#define SECUREBOX_UTILS_H

#include <pthread.h>
#include <time.h>
#include <sys/types.h>

#include "config.h"

/* ----- Structure decrivant un evenement detecte ----- */
typedef struct {
    event_type_t type;
    char         path[MAX_PATH_LEN];   /* chemin complet de la ressource */
    char         old_path[MAX_PATH_LEN]; /* utilise pour le renommage */
    time_t       timestamp;
    int          is_dir;
} event_t;

/* ----- File d'evenements circulaire thread-safe ----- */
typedef struct {
    event_t          buffer[EVENT_QUEUE_CAPACITY];
    int              head;
    int              tail;
    int              count;
    int              closed;        /* mis a 1 lors de l'arret */
    pthread_mutex_t  mutex;
    pthread_cond_t   not_empty;
    pthread_cond_t   not_full;
} event_queue_t;

/* Initialisation / destruction */
int  queue_init(event_queue_t *q);
void queue_destroy(event_queue_t *q);

/* Ajoute un evenement (bloque si la file est pleine). Retourne 0 en cas de
 * succes, -1 si la file a ete fermee. */
int  queue_push(event_queue_t *q, const event_t *ev);

/* Recupere un evenement (bloque tant que la file est vide). Retourne 0 si un
 * evenement a ete recupere, -1 si la file a ete fermee et videe. */
int  queue_pop(event_queue_t *q, event_t *out);

/* Marque la file comme fermee et reveille toutes les threads bloquees. */
void queue_close(event_queue_t *q);

/* ----- Fonctions utilitaires generales ----- */

/* Remplit `buf` avec un horodatage humain "YYYY-MM-DD HH:MM:SS". */
void utils_format_timestamp(time_t t, char *buf, size_t buf_size);

/* Verifie que `path` existe et est un repertoire accessible.
 * Retourne 0 si OK, -1 sinon (un message d'erreur est ecrit sur stderr). */
int  utils_check_directory(const char *path);

/* Renvoie 1 si le nom de fichier commence par '.' (fichier cache), 0 sinon. */
int  utils_is_hidden(const char *name);

/* Renvoie 1 si le fichier porte une extension interdite (voir config.h). */
int  utils_has_forbidden_extension(const char *name);

/* Convertit un type d'evenement en chaine ("CREATE_FILE", ...). */
const char *utils_event_type_str(event_type_t t);

/* Extrait le nom de base d'un chemin (pointeur dans la chaine d'entree). */
const char *utils_basename(const char *path);

#endif /* SECUREBOX_UTILS_H */
