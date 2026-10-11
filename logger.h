/*
 * logger.h - Module de journalisation thread-safe.
 *
 * Ouvre le fichier journal en mode "append", protege les ecritures par un
 * mutex et flush apres chaque ligne pour eviter toute perte en cas d'arret
 * brutal. Le format de chaque entree est :
 *   [YYYY-MM-DD HH:MM:SS] [INFO|ALERTE] TYPE_EVENEMENT : /chemin/ressource
 */

#ifndef SECUREBOX_LOGGER_H
#define SECUREBOX_LOGGER_H

#include "config.h"
#include "utils.h"

/* Ouvre/cree le fichier log `path`. Retourne 0 si OK, -1 en cas d'erreur. */
int  logger_init(const char *path);

/* Ferme proprement le fichier log. */
void logger_close(void);

/* Ecrit une entree formatee dans le journal et l'echoue sur stdout. */
void logger_log_event(const event_t *ev, severity_t sev);

/* Ecrit un message libre (ex. demarrage / arret / alerte agregee). */
void logger_log_message(severity_t sev, const char *fmt, ...);

#endif /* SECUREBOX_LOGGER_H */
