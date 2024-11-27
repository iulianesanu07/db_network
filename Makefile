# Nom de l'exécutable final
EXEC = app

# Répertoires
SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = include
LIB_DIR = lib

# Bibliothèques à lier
LIBS = -L$(LIB_DIR) -lpq

# Trouver tous les fichiers source dans le répertoire src
SRC = $(wildcard $(SRC_DIR)/*.c)

# Générer les noms correspondants pour les fichiers objets dans build
OBJ = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))

# Compilateur et options
CC = clang
CFLAGS = -Wall -Wextra -std=c11 -I$(INCLUDE_DIR)

# Règle par défaut : construire l'exécutable
$(EXEC): $(OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LIBS)

# Compiler chaque fichier source en fichier objet
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Nettoyage des fichiers objets et de l'exécutable
clean:
	rm -rf $(BUILD_DIR) $(EXEC)

# Phony targets pour éviter des conflits avec des fichiers portant le même nom
.PHONY: clean
