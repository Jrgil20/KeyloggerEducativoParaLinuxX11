# Makefile para X11 Educational Keylogger

CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lX11 -lXtst
TARGET = x11_keylogger
SOURCE = x11_keylogger.c

# Colores para output
RED = \033[0;31m
GREEN = \033[0;32m
YELLOW = \033[1;33m
NC = \033[0m # No Color

.PHONY: all clean install uninstall help

all: $(TARGET)

$(TARGET): $(SOURCE)
	@echo "$(GREEN)Compilando $(TARGET)...$(NC)"
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)
	@echo "$(GREEN)✓ Compilación exitosa!$(NC)"
	@echo "$(YELLOW)Ejecute './$(TARGET)' para iniciar el keylogger$(NC)"

clean:
	@echo "$(YELLOW)Limpiando archivos compilados...$(NC)"
	rm -f $(TARGET)
	rm -f *.o
	@echo "$(GREEN)✓ Limpieza completa$(NC)"

# Instalar (requiere sudo)
install: $(TARGET)
	@echo "$(YELLOW)Instalando $(TARGET)...$(NC)"
	install -m 755 $(TARGET) /usr/local/bin/
	@echo "$(GREEN)✓ Instalado en /usr/local/bin/$(TARGET)$(NC)"

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
	@echo "  make          - Compilar el keylogger"
	@echo "  make clean    - Eliminar archivos compilados"
	@echo "  make install  - Instalar en /usr/local/bin (requiere sudo)"
	@echo "  make uninstall- Desinstalar de /usr/local/bin (requiere sudo)"
	@echo "  make help     - Mostrar esta ayuda"
	@echo ""
	@echo "$(YELLOW)Requisitos:$(NC)"
	@echo "  - Sistema Linux con X11"
	@echo "  - gcc compilador"
	@echo "  - libx11-dev (desarrollo de Xlib)"
	@echo ""
	@echo "$(RED)ADVERTENCIA:$(NC)"
	@echo "  Este programa es solo para propósitos educativos."
	@echo "  El uso no autorizado puede ser ilegal."
