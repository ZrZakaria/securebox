/*
 * config.h - Constantes de configuration du systeme SecureBox
 *
 * Centralise toutes les valeurs de configuration utilisees par les modules :
 * chemins, tailles de buffers, seuils de detection d'anomalies, extensions
 * interdites, etc.
 */

#ifndef SECUREBOX_CONFIG_H
#define SECUREBOX_CONFIG_H

/* ----- Chemins par defaut ----- */
#define LOG_DIR              "logs"
#define LOG_FILE             "logs/securebox.log"

/* ----- Tailles ----- */
#define MAX_PATH_LEN         4096
#define MAX_NAME_LEN         512
#define MAX_MSG_LEN          1024
#define EVENT_QUEUE_CAPACITY 1024

/* Buffer pour la lecture inotify (doit etre aligne et suffisant) */
#define INOTIFY_BUF_LEN      (64 * (sizeof(struct inotify_event) + 256))

/* ----- Seuils de detection d'anomalies ----- */
/* Plus de DELETE_THRESHOLD suppressions en DELETE_WINDOW_SEC secondes -> alerte */
#define DELETE_THRESHOLD     5
#define DELETE_WINDOW_SEC    10

/* Plus de MODIFY_THRESHOLD modifications d'un meme fichier en MODIFY_WINDOW_SEC secondes */
#define MODIFY_THRESHOLD     3
#define MODIFY_WINDOW_SEC    5

/* Taille maximale du tableau interne suivant les fichiers recents */
#define MAX_TRACKED_FILES    256

/* ----- Liste noire d'extensions interdites ----- */
/* Tout fichier cree avec l'une de ces extensions declenche une alerte. */
#define FORBIDDEN_EXTENSIONS_COUNT 6
static const char *const FORBIDDEN_EXTENSIONS[FORBIDDEN_EXTENSIONS_COUNT] = {
    ".exe", ".bat", ".sh", ".cmd", ".dll", ".scr"
};

/* ----- Niveaux de gravite ----- */
typedef enum {
    SEV_INFO   = 0,
    SEV_ALERTE = 1
} severity_t;

/* ----- Types d'evenement ----- */
typedef enum {
    EVT_UNKNOWN = 0,
    EVT_CREATE_FILE,
    EVT_DELETE_FILE,
    EVT_MODIFY_FILE,
    EVT_RENAME_FILE,
    EVT_CREATE_DIR,
    EVT_DELETE_DIR,
    EVT_MOVE_INTERNAL
} event_type_t;

#endif /* SECUREBOX_CONFIG_H */
