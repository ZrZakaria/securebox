/*
 * monitor.c - Implementation du module de surveillance inotify.
 *
 * La boucle utilise poll() avec un court timeout pour pouvoir consulter
 * regulierement le drapeau g_stop_requested (defini dans signals.c) sans
 * etre bloquee indefiniment dans read(). Les evenements inotify sont
 * convertis en event_t puis pousses dans la file ; les renommages a
 * l'interieur du repertoire sont reconstruits en associant les
 * IN_MOVED_FROM / IN_MOVED_TO partageant le meme cookie.
 */

#define _POSIX_C_SOURCE 200809L

#include "monitor.h"
#include "logger.h"
#include "signals.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#define WATCH_MASK ( IN_CREATE | IN_DELETE | IN_MODIFY | \
                     IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB | \
                     IN_CLOSE_WRITE )

int monitor_init(monitor_ctx_t *ctx, const char *target_dir,
                 event_queue_t *queue)
{
    if (ctx == NULL || target_dir == NULL || queue == NULL) return -1;
    memset(ctx, 0, sizeof(*ctx));
    snprintf(ctx->target_dir, sizeof(ctx->target_dir), "%s", target_dir);
    ctx->queue      = queue;
    ctx->inotify_fd = -1;
    ctx->watch_fd   = -1;

    ctx->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (ctx->inotify_fd < 0) {
        fprintf(stderr, "[ERREUR] inotify_init1 : %s\n", strerror(errno));
        return -1;
    }
    ctx->watch_fd = inotify_add_watch(ctx->inotify_fd, target_dir, WATCH_MASK);
    if (ctx->watch_fd < 0) {
        fprintf(stderr, "[ERREUR] inotify_add_watch sur '%s' : %s\n",
                target_dir, strerror(errno));
        close(ctx->inotify_fd);
        ctx->inotify_fd = -1;
        return -1;
    }
    return 0;
}

void monitor_close(monitor_ctx_t *ctx)
{
    if (ctx == NULL) return;
    if (ctx->inotify_fd >= 0 && ctx->watch_fd >= 0) {
        inotify_rm_watch(ctx->inotify_fd, ctx->watch_fd);
    }
    if (ctx->inotify_fd >= 0) {
        close(ctx->inotify_fd);
    }
    ctx->inotify_fd = -1;
    ctx->watch_fd   = -1;
}

/* Construit un chemin complet en concatenant le repertoire surveille et
 * le nom recu d'inotify (qui est uniquement le basename). */
static void build_full_path(const char *dir, const char *name,
                            char *out, size_t out_size)
{
    if (name == NULL || name[0] == '\0') {
        snprintf(out, out_size, "%s", dir);
    } else {
        snprintf(out, out_size, "%s/%s", dir, name);
    }
}

/* Etat utilise pour apparier les renommages internes (cookie inotify). */
typedef struct {
    uint32_t cookie;
    char     old_path[MAX_PATH_LEN];
    int      is_dir;
} pending_move_t;

static void push_event(event_queue_t *q, event_type_t type, int is_dir,
                       const char *path, const char *old_path)
{
    event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type      = type;
    ev.timestamp = time(NULL);
    ev.is_dir    = is_dir;
    snprintf(ev.path, sizeof(ev.path), "%s", path);
    if (old_path != NULL) {
        snprintf(ev.old_path, sizeof(ev.old_path), "%s", old_path);
    }
    /* Ignorer silencieusement si la file est fermee. */
    (void)queue_push(q, &ev);
}

void *monitor_run(void *arg)
{
    monitor_ctx_t *ctx = (monitor_ctx_t *)arg;
    if (ctx == NULL) return NULL;

    char buf[INOTIFY_BUF_LEN]
        __attribute__ ((aligned(__alignof__(struct inotify_event))));

    pending_move_t pending = { 0, "", 0 };
    struct pollfd  pfd     = { .fd = ctx->inotify_fd, .events = POLLIN };

    logger_log_message(SEV_INFO,
        "Surveillance demarree sur %s (pid=%ld)",
        ctx->target_dir, (long)getpid());

    while (!signals_stop_requested()) {
        int pret = poll(&pfd, 1, 500); /* timeout 500 ms */
        if (pret < 0) {
            if (errno == EINTR) continue;
            logger_log_message(SEV_ALERTE,
                "Erreur poll() : %s", strerror(errno));
            break;
        }
        if (pret == 0) continue; /* pas d'evenement, on reverifie le flag */
        if (!(pfd.revents & POLLIN)) continue;

        ssize_t n = read(ctx->inotify_fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR) continue;
            logger_log_message(SEV_ALERTE,
                "Erreur read inotify : %s", strerror(errno));
            break;
        }
        if (n == 0) continue;

        for (char *p = buf; p < buf + n; ) {
            struct inotify_event *e = (struct inotify_event *)p;
            char full[MAX_PATH_LEN];
            build_full_path(ctx->target_dir, e->name, full, sizeof(full));

            int is_dir = (e->mask & IN_ISDIR) ? 1 : 0;

            if (e->mask & IN_CREATE) {
                push_event(ctx->queue,
                    is_dir ? EVT_CREATE_DIR : EVT_CREATE_FILE,
                    is_dir, full, NULL);
            } else if (e->mask & IN_DELETE) {
                push_event(ctx->queue,
                    is_dir ? EVT_DELETE_DIR : EVT_DELETE_FILE,
                    is_dir, full, NULL);
            } else if ((e->mask & IN_MODIFY) || (e->mask & IN_CLOSE_WRITE)) {
                if (!is_dir) {
                    push_event(ctx->queue, EVT_MODIFY_FILE, 0, full, NULL);
                }
            } else if (e->mask & IN_MOVED_FROM) {
                pending.cookie = e->cookie;
                pending.is_dir = is_dir;
                snprintf(pending.old_path, sizeof(pending.old_path), "%s", full);
            } else if (e->mask & IN_MOVED_TO) {
                if (pending.cookie != 0 && pending.cookie == e->cookie) {
                    push_event(ctx->queue,
                        EVT_RENAME_FILE, pending.is_dir,
                        full, pending.old_path);
                    pending.cookie = 0;
                    pending.old_path[0] = '\0';
                } else {
                    /* Fichier deplace depuis l'exterieur du repertoire. */
                    push_event(ctx->queue,
                        EVT_MOVE_INTERNAL, is_dir, full, NULL);
                }
            } else if (e->mask & IN_ATTRIB) {
                /* Changement de droits / d'horodatage : utile pour
                 * detecter l'apparition d'un bit executable. */
                push_event(ctx->queue, EVT_MODIFY_FILE, is_dir, full, NULL);
            }

            p += sizeof(struct inotify_event) + e->len;
        }
    }

    logger_log_message(SEV_INFO, "Thread de surveillance termine.");
    return NULL;
}
