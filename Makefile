# Makefile para X11 Educational Keylogger

CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lX11 -lXtst

# Directorios
SRC_DIR = src
BIN_DIR = bin
SCRIPTS_DIR = scripts

TARGET = $(BIN_DIR)/x11_keylogger
SOURCE = $(SRC_DIR)/x11_keylogger.c

# Colores para output
GREEN = \033[0;32m
YELLOW = \033[1;33m
RED = \033[0;31m
NC = \033[0m

.PHONY: all clean install uninstall help run stop

all: $(TARGET)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(TARGET): $(SOURCE) | $(BIN_DIR)
	@echo "$(GREEN)Compilando...$(NC)"
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)
	@echo "$(GREEN)✓ Compilado en $(TARGET)$(NC)"

clean:
	@echo "$(YELLOW)Limpiando...$(NC)"
	rm -f $(TARGET)
	@echo "$(GREEN)✓ Limpio$(NC)"

run: $(TARGET)
	@chmod +x $(SCRIPTS_DIR)/run.sh
	@$(SCRIPTS_DIR)/run.sh

stop:
	@chmod +x $(SCRIPTS_DIR)/stop.sh
	@$(SCRIPTS_DIR)/stop.sh

install: $(TARGET)
	@echo "$(YELLOW)Instalando...$(NC)"
	install -m 755 $(TARGET) /usr/local/bin/x11_keylogger
	@echo "$(GREEN)✓ Instalado en /usr/local/bin/x11_keylogger$(NC)"

uninstall:
	rm -f /usr/local/bin/x11_keylogger
	@echo "$(GREEN)✓ Desinstalado$(NC)"

help:
	@echo "$(GREEN)X11 Educational Keylogger$(NC)"
	@echo ""
	@echo "Uso:"
	@echo "  make          Compilar"
	@echo "  make run      Compilar y ejecutar (daemon)"
	@echo "  make stop     Detener el daemon"
	@echo "  make clean    Limpiar binarios"
	@echo "  make install  Instalar en sistema"
	@echo "  make help     Esta ayuda"
