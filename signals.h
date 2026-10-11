/*
 * signals.h - Gestion centralisee des signaux systeme.
 *
 *   SIGINT, SIGTERM : demande d'arret propre (positionne un drapeau
 *                     atomique consulte par les boucles principales).
 *   SIGUSR1        : declenche l'affichage des statistiques (sans
 *                     interrompre l'execution).
 *
 * Les handlers sont volontairement minimaux : ils n'utilisent que des
 * fonctions asynchrones-signal-safe. Le travail effectif (impression,
 * destruction) est realise par le thread principal apres reveil.
 */

#ifndef SECUREBOX_SIGNALS_H
#define SECUREBOX_SIGNALS_H

#include "stats.h"
#include "utils.h"

/* Installe les handlers et associe les structures partagees a utiliser
 * pour reveiller la boucle (la file est fermee lors d'un SIGINT/TERM). */
int signals_install(stats_t *stats, event_queue_t *queue);

/* Renvoie un entier non nul si un arret a ete demande. */
int signals_stop_requested(void);

/* Renvoie un entier non nul (et reinitialise le drapeau) si une demande
 * de stats a ete recue. */
int signals_consume_stats_request(void);

#endif /* SECUREBOX_SIGNALS_H */
