# =============================================================================
#  Makefile - SecureBox
#  Compilation : `make`         -> produit ./securebox
#  Nettoyage   : `make clean`   -> supprime les .o et l'executable
#  Debug       : `make debug`   -> compile avec -g et symboles
#  Lancement   : `make run DIR=/chemin`
# =============================================================================

CC       := gcc
CFLAGS   := -std=c11 -Wall -Wextra -Wpedantic -Wshadow -D_POSIX_C_SOURCE=200809L
LDFLAGS  :=
LDLIBS   := -lpthread

SRCS     := main.c monitor.c logger.c analyzer.c stats.c signals.c utils.c
OBJS     := $(SRCS:.c=.o)
HEADERS  := config.h monitor.h logger.h analyzer.h stats.h signals.h utils.h
TARGET   := securebox

# Cible par defaut --------------------------------------------------------------
all: $(TARGET)

# Edition de liens --------------------------------------------------------------
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	@mkdir -p logs

# Regle generique de compilation -----------------------------------------------
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Build avec symboles de debogage ----------------------------------------------
debug: CFLAGS += -g -O0 -DDEBUG
debug: clean all

# Build avec optimisations -----------------------------------------------------
release: CFLAGS += -O2
release: clean all

# Execution rapide -------------------------------------------------------------
run: all
	@if [ -z "$(DIR)" ]; then \
		echo "Utilisation : make run DIR=/chemin/repertoire"; \
		exit 1; \
	fi
	./$(TARGET) $(DIR)

# Verification valgrind --------------------------------------------------------
memcheck: debug
	@if [ -z "$(DIR)" ]; then \
		echo "Utilisation : make memcheck DIR=/chemin/repertoire"; \
		exit 1; \
	fi
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET) $(DIR)

# Nettoyage --------------------------------------------------------------------
clean:
	rm -f $(OBJS) $(TARGET)

distclean: clean
	rm -f logs/securebox.log

.PHONY: all clean distclean debug release run memcheck
