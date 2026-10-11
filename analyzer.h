/*
 * analyzer.h - Module d'analyse et de detection d'activites suspectes.
 *
 * Etudie chaque evenement contre un ensemble de regles :
 *   - rafale de suppressions ;
 *   - modifications rapides repetees sur un meme fichier ;
 *   - apparition de fichiers caches ;
 *   - creations avec extension listee dans la liste noire.
 *
 * Renvoie un niveau de gravite : SEV_INFO ou SEV_ALERTE et, en cas
 * d'alerte, fournit un message lisible decrivant la regle declenchee.
 */

#ifndef SECUREBOX_ANALYZER_H
#define SECUREBOX_ANALYZER_H

#include "config.h"
#include "utils.h"

void analyzer_init(void);
void analyzer_destroy(void);

/*
 * Analyse l'evenement `ev`. Si une regle est declenchee, `out_alert_msg`
 * (taille `msg_size`) recoit un message descriptif (sinon il est laisse
 * vide). La fonction retourne le niveau de gravite a appliquer pour
 * l'entree de journal correspondante.
 */
severity_t analyzer_check(const event_t *ev,
                          char *out_alert_msg, size_t msg_size);

#endif /* SECUREBOX_ANALYZER_H */
