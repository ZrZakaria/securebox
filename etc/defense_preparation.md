# 🎓 Préparation Soutenance — Half B SecureBox (Aligné sur le Cahier des Charges)

Ce document de préparation a été révisé pour faire explicitement le lien avec le document de spécifications **"Projet : Conception et réalisation d’un système de surveillance sécurisée d’un répertoire sous Linux" (Projet2 (1).pdf)**.

---

## Partie 1 : Script de Présentation (en français)

> Durée estimée : 8-10 minutes pour ton Half

---

### 🎤 Introduction (30 secondes)

> « Je vais maintenant présenter le sous-système d'analyse et de journalisation de SecureBox. Mon binôme a montré comment les événements sont capturés via inotify, ce qui répond à l'**objectif de surveillance temps réel (Section 4 du cahier des charges)**. Moi, je vais expliquer **ce qui se passe de l'autre côté de la file d'événements partagée (IPC)**.
>
> Mon travail couvre trois besoins fonctionnels majeurs du projet (Section 9) :
> 1. Mémoriser les événements dans un fichier journal.
> 2. Distinguer les événements normaux des événements suspects (Analyse de sécurité).
> 3. Permettre la consultation de statistiques et l'arrêt propre.
>
> Tout cela s'inscrit dans l'**Option 2 de l'architecture technique (Section 13) : Architecture multi-threads avec synchronisation par mutex et variables de condition.** »

---

### 🎤 Slide 1 : config.h — Les constantes (1 minute)

> « Tout commence par `config.h`, qui centralise **toutes** les valeurs configurables du projet. C'est ici que j'ai défini les paramètres dictés par le cahier des charges :
>
> - Les **seuils de détection (Section 10.4 - Analyse de sécurité)** : `DELETE_THRESHOLD` à 5 et `DELETE_WINDOW_SEC` à 10 pour la "suppression répétée", et `MODIFY_THRESHOLD` à 3 et `MODIFY_WINDOW_SEC` à 5 pour les "modifications multiples".
> - La **liste noire d'extensions interdites** : `.exe`, `.bat`, `.sh`, `.cmd`, `.dll`, `.scr`, comme demandé dans les "Exemples de règles".
> - Le **format du journal (Section 10.3)** : La taille maximale des chemins (`MAX_PATH_LEN = 4096`) et des messages, ainsi que les niveaux de gravité (`SEV_INFO`, `SEV_ALERTE`).
>
> L'intérêt d'avoir tout dans un seul header, c'est qu'on peut ajuster les critères de sécurité du cahier des charges sans toucher à la logique d'implémentation. »

---

### 🎤 Slide 2 : stats.c — Le modèle mutex et les Statistiques (2 minutes)

> « Le cahier des charges exige, à la **Section 10.6 (Statistiques)**, de maintenir et d'afficher à la demande (SIGUSR1) le nombre total d'événements, les créations, suppressions, modifications, renommages, alertes, et la durée.
>
> J'ai implémenté cela dans `stats.c` en utilisant un **modèle de mutex**, respectant ainsi les contraintes de synchronisation de l'architecture multi-threads (Section 13).
>
> La struct `stats_t` contient tous ces compteurs et un **mutex intégré**.
>
> Le pattern est toujours le même :
> 1. `pthread_mutex_lock` — on entre en section critique
> 2. On modifie ou on lit les données
> 3. `pthread_mutex_unlock` — on sort
>
> **Pourquoi c'est nécessaire ?** Parce que le thread d'analyse incrémente les compteurs pendant que le thread principal peut lire et afficher ces mêmes compteurs via la fonction `stats_print()`. La lecture sous mutex garantit un **instantané cohérent** des statistiques, évitant d'afficher des totaux qui ne correspondent pas à la somme des sous-compteurs. »

---

### 🎤 Slide 3 : logger.c — La Journalisation fiable (2-3 minutes)

> « La **Section 10.3 (Journalisation)** du projet impose des contraintes fortes : créer le fichier s'il n'existe pas, avoir des écritures fiables et ordonnées, et surtout, **ne perdre aucun log lors de l'arrêt**. Le format imposé est `[date heure] [INFO/ALERTE] TYPE : chemin`.
>
> J'ai conçu le module `logger.c` pour répondre exactement à cela. Il utilise **deux poignées** pour le même fichier :
> - Un `FILE*` pour formater facilement la date, le type et le chemin avec `fprintf`.
> - Un descripteur brut `int fd` pour garantir la persistance physique avec `fsync`.
>
> À l'ouverture, j'utilise `open()` avec le flag `O_APPEND`. C'est crucial pour la contrainte "écritures fiables" en multi-threading : POSIX **garantit** que chaque écriture avec `O_APPEND` est atomique.
>
> La fonction centrale `write_line` fait 3 choses dans son propre mutex (`g_log_mutex`) :
> 1. `fprintf` — écrit dans le buffer C (format attendu).
> 2. `fflush` — pousse le buffer vers le noyau (évite la perte en cas de crash applicatif).
> 3. `fsync` — force le noyau à écrire sur le **disque physique** (garantit la persistance, même en cas de coupure de courant).
>
> Enfin, conformément à la **Section 10.5 (Génération d'alertes)**, les alertes sont affichées sur la console (via `stderr`) tout en étant écrites dans le log, couvrant ainsi les deux formes demandées. »

---

### 🎤 Slide 4 : analyzer.c — L'Analyse de Sécurité (3 minutes)

> « Le "Module 3 : Analyse" (Section 12) et l'"Analyse de sécurité" (Section 10.4) sont le cœur de ma partie. L'analyseur vérifie chaque événement contre les **5 situations suspectes** listées dans le cahier des charges.
>
> Pour les deux premières règles temporelles, j'ai implémenté un algorithme de **fenêtre glissante** (sliding window).
>
> 1. **Rafale de suppressions (> 5 en < 10s)** : J'utilise une fenêtre globale (`g_del_window`). À chaque événement, je "compacte" le tableau en retirant les timestamps de plus de 10 secondes. Si le compteur dépasse le seuil, je génère une alerte.
> 2. **Modifications rapides répétées (> 3 en < 5s sur un MÊME fichier)** : Ici, j'utilise un tableau de suivi par fichier (`modify_track_t`), limité à 256 fichiers simultanés pour des raisons de performance.
> 3. **Création de fichier caché** : Simple vérification si le nom (extrait avec `utils_basename`) commence par `.`.
> 4. **Extension interdite** : Vérification par rapport au tableau `FORBIDDEN_EXTENSIONS` de la liste noire.
> 5. **Fichier exécutable** : Apparition ou déplacement d'un exécutable. J'utilise l'appel système `lstat` (demandé en Section 14.4) pour vérifier les permissions d'exécution (les bits `+x`), sans suivre les liens symboliques par sécurité.
>
> Ces 5 règles sont vérifiées séquentiellement. La condition `sev == SEV_INFO` agit comme un court-circuit : une seule alerte maximum est remontée par événement, gardant le système "fluide" (Section 11.1). »

---

### 🎤 Slide 5 : main.c — L'Architecture Multi-Threads et l'Arrêt Propre (1 minute)

> « Pour orchestrer tout cela (Module 1 de la Section 12), `main.c` met en place l'**Architecture multi-threads (Option 2)**.
>
> Le `main` initialise les modules dans l'ordre strict de leurs dépendances. Un point technique important : la file d'événements (`event_queue_t`) est déclarée en `static` car elle est trop volumineuse (~8 Mo) pour la pile standard d'un thread Linux.
>
> Le thread principal lance ensuite `monitor_run` et `analyzer_thread` (avec `pthread_create`, Section 14.4).
>
> Enfin, pour satisfaire la **Section 10.7 (Arrêt propre)**, le thread principal gère les signaux (interception de `SIGINT`/`SIGTERM` via `sigaction`). Lors d'un `Ctrl+C`, il déclenche un arrêt propre : il ferme la file (`queue_close`), fait un `pthread_join` sur les threads pour éviter les fuites, affiche le résumé final des statistiques, et ferme les fichiers ouverts (`logger_close`). »

---

### 🎤 Conclusion (20 secondes)

> « En résumé, ma partie couvre entièrement les spécifications d'analyse et de journalisation du cahier des charges :
> 1. **Sécurité et Détection** — Implémentation des 5 règles d'anomalie demandées (Section 10.4).
> 2. **Fiabilité** — Utilisation stricte de mutex pour la synchronisation multi-threads (Option 2) et de `fsync` pour des logs garantis (Section 10.3).
> 3. **Propreté** — Un arrêt sans fuite de mémoire ni ressources orphelines (Section 10.7).
>
> Je vous remercie. Avez-vous des questions ? »

---

## Partie 2 : Le lien direct entre votre code et les exigences du PDF

Voici un tableau de correspondance rapide, très utile si le professeur demande "Où avez-vous traité l'exigence X ?"

| Exigence du PDF | Emplacement dans votre code | Explication Technique |
| :--- | :--- | :--- |
| **10.3 Journalisation :** Création si n'existe pas | `logger.c : ensure_parent_dir()` et `open(O_CREAT)` | Crée `logs/` et le fichier `securebox.log`. |
| **10.3 Journalisation :** Format `[Date] [INFO] TYPE: chemin` | `logger.c : write_line()` et `logger_log_event()` | Utilisation de `utils_format_timestamp()` et `snprintf()`. |
| **10.3 Journalisation :** Écritures fiables / pas de perte | `logger.c : open(O_APPEND)` et `write_line()` | `O_APPEND` pour l'atomicité multi-processus, `fflush`+`fsync` pour forcer l'écriture disque. |
| **10.4 Analyse :** >5 suppressions en <10s | `analyzer.c : record_and_count_deletions()` | Algorithme de fenêtre glissante (`timestamps`, compactage in-place). |
| **10.4 Analyse :** >3 modifs en <5s (même fichier) | `analyzer.c : record_and_count_modifications()` | Fenêtre glissante couplée à un tableau de suivi par fichier (`modify_track_t`). |
| **10.4 Analyse :** Fichier caché | `analyzer.c : analyzer_check()` et `utils.c : utils_is_hidden()` | Vérification `name[0] == '.'`. |
| **10.4 Analyse :** Extension interdite | `analyzer.c` et `config.h : FORBIDDEN_EXTENSIONS` | Comparaison avec la liste noire définie dans le `.h`. |
| **10.4 Analyse :** Script exécutable | `analyzer.c : is_executable_file()` | Appel système `lstat()`, test des bits de droits (`S_IXUSR`, `S_IXGRP`, `S_IXOTH`). |
| **10.5 Alertes :** Affichage console + journal | `logger.c : write_line()` | `fprintf` sur `g_log_fp` (fichier) ET `fprintf` sur `stderr` ou `stdout` (console). |
| **10.6 Statistiques :** Compteurs et affichage sur signal | `stats.c` et `signals.c : on_stats()` | Structure `stats_t` protégée par mutex. Handler SIGUSR1 qui lève un drapeau. |
| **10.7 Arrêt propre :** Fermer fichiers, libérer, join | `main.c` (lignes 181-201) | Séquence : `queue_close`, `pthread_join`, `stats_destroy`, `logger_close`. |
| **13. Option 2 :** Multi-threads avec mutex/cond | Partout (`main.c`, `stats.c`, `utils.c`) | Utilisation de `pthread_mutex_t` et `pthread_cond_t` (`event_queue_t`). |
| **14.4 Appels système :** `lstat()`, `pthread_*` | `analyzer.c`, `main.c`, `stats.c`, `utils.c` | Strict respect des fonctions POSIX autorisées. Interdiction de `system()` respectée. |

---

## Partie 3 : Deep-Dive sur les 3 concepts fondamentaux

*(Même contenu que la version précédente, ces concepts restent les clés techniques de l'implémentation de votre cahier des charges).*

### Concept A : Le Mutex (Mutual Exclusion)

**Règle d'or :** Toute donnée partagée entre threads doit être accédée sous le même mutex (Requis par l'Architecture Option 2).

**3 patterns dans le projet :**
| Module | Variable protégée | Mutex |
|--------|-------------------|-------|
| `stats.c` | `stats_t` (compteurs) | `s->mutex` (dans la struct) |
| `logger.c` | `g_log_fp`, `g_log_fd` | `g_log_mutex` (global statique) |
| `analyzer.c` | `g_del_window`, `g_mod_track` | `g_mutex` (global statique) |

---

### Concept B : La Fenêtre Glissante (Sliding Window)

C'est l'algorithme qui permet de valider les règles complexes d'anomalies de temps de la Section 10.4.

**Algorithme (en pseudo-code) :**
```
fonction enregistrer_et_compter(now):
    // 1. Compacter : supprimer les vieux
    kept = 0
    pour chaque timestamp dans le tableau:
        si (now - timestamp) <= WINDOW_SEC:
            tableau[kept] = timestamp  // garder en place
            kept++
    
    // 2. Ajouter le nouveau
    si kept < capacité:
        tableau[kept] = now
        kept++
    
    // 3. Mettre à jour et retourner
    count = kept
    retourner kept
```

---

### Concept C : L'I/O Crash-Safe (O_APPEND + fflush + fsync)

Permet de valider la contrainte "les logs ne doivent pas être perdus lors de l'arrêt" de la Section 10.3.

**Les 4 couches de protection dans logger.c :**
```
 Programme C                    Noyau Linux                Disque
 ┌──────────┐   fflush()    ┌──────────────┐  fsync()   ┌──────┐
 │ buffer    │ ───────────► │  page cache   │ ─────────► │ SSD  │
 │ stdio     │              │  (RAM noyau)  │            │      │
 └──────────┘               └──────────────┘             └──────┘
```
- **Sans `fflush` :** Les données restent dans le buffer userspace.
- **Sans `fsync` :** Les données sont dans le page cache.
- **Avec `O_APPEND` :** POSIX garantit l'atomicité du "seek-to-end + write", même si un autre script écrit dans le fichier.

---

## Partie 4 : Questions d'entraînement adaptées au Cahier des Charges

### Q1 : Comment avez-vous satisfait l'exigence de la section 10.3 demandant que "les logs ne doivent pas être perdus lors de l'arrêt" ?
**R :** En utilisant un mécanisme à trois niveaux dans `logger.c`. Après chaque écriture C (`fprintf`), j'appelle `fflush` pour vider le buffer applicatif vers le noyau, puis `fsync` sur le descripteur de fichier brut pour forcer le noyau à écrire physiquement sur le disque.

### Q2 : Pourquoi l'architecture multi-threads (Option 2) exigeait-elle l'utilisation de mutex dans le module des statistiques ?
**R :** Le thread d'analyse (`analyzer_thread`) incrémente constamment les statistiques à chaque événement. Simultanément, si l'utilisateur envoie le signal `SIGUSR1` (Section 10.6), le thread principal va lire ces mêmes compteurs pour les afficher. Le mutex empêche le thread principal de lire un état intermédiaire ou corrompu (torn read) pendant que l'analyseur est en train de modifier plusieurs compteurs.

### Q3 : La section 10.4 demande de détecter une "suppression répétée dans un court intervalle". Comment l'avez-vous programmé sans surcharger le CPU ?
**R :** J'ai utilisé un algorithme de fenêtre glissante (sliding window) de 10 secondes dans `analyzer.c`. Je stocke uniquement les horodatages (timestamps) des dernières suppressions. À chaque nouvelle suppression, je parcours ce petit tableau (maximum 20 éléments) pour supprimer les timestamps vieux de plus de 10 secondes (compactage in-place), j'ajoute le nouveau, et je compte. C'est en `O(n)` avec `n` très petit, donc extrêmement rapide.

### Q4 : Quelle est la différence entre `stat()` et `lstat()` que vous utilisez pour détecter les exécutables (Section 10.4) ?
**R :** `stat()` suit les liens symboliques, tandis que `lstat()` examine le lien lui-même. Dans un contexte de sécurité, utiliser `lstat()` est primordial : si un attaquant crée un simple raccourci (lien) vers `/bin/bash` dans notre répertoire, `stat()` verrait les droits de `bash` et déclencherait une alerte sur le lien. `lstat()` permet de vérifier uniquement le fichier présent dans notre dossier.

### Q5 : Comment avez-vous garanti l'"arrêt propre" demandé par la section 10.7 ?
**R :** Lorsqu'un signal `SIGINT` (Ctrl+C) est reçu, un drapeau est levé. Le thread principal sort de sa boucle et déclenche la séquence de libération (lignes 181-201 de `main.c`). La clé est d'appeler `queue_close()`, qui débloque les threads en attente sur les variables de condition. Ensuite, on fait un `pthread_join` sur les threads pour s'assurer qu'ils ont terminé. Enfin, on libère les mutex et on ferme le fichier log proprement. Le programme se termine donc sans aucune ressource système orpheline.
