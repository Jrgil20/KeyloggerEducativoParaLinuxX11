#!/bin/bash

# Script de demostración para X11 Educational Keylogger
# Este script ayuda a demostrar las vulnerabilidades de X11 de forma segura

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}"
echo "═══════════════════════════════════════════════════════════════"
echo "  X11 Educational Keylogger - Script de Demostración"
echo "═══════════════════════════════════════════════════════════════"
echo -e "${NC}"

# Verificar si estamos en X11
check_x11() {
    echo -e "\n${YELLOW}[1/5] Verificando entorno gráfico...${NC}"
    
    if [ -z "$DISPLAY" ]; then
        echo -e "${RED}✗ Error: No se detectó servidor X activo${NC}"
        echo -e "${YELLOW}  Este programa requiere un entorno X11 activo${NC}"
        exit 1
    fi
    
    if [ "$XDG_SESSION_TYPE" = "wayland" ]; then
        echo -e "${RED}✗ Advertencia: Estás usando Wayland${NC}"
        echo -e "${YELLOW}  Este keylogger solo funciona en X11${NC}"
        echo -e "${YELLOW}  Wayland es seguro y no es vulnerable a este ataque${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}✓ Servidor X11 detectado en: $DISPLAY${NC}"
    echo -e "${RED}  ⚠ Tu sistema ES VULNERABLE a este tipo de keylogger${NC}"
}

# Verificar compilación
check_compilation() {
    echo -e "\n${YELLOW}[2/5] Verificando compilación...${NC}"
    
    if [ ! -f "x11_keylogger" ]; then
        echo -e "${YELLOW}  El keylogger no está compilado. Compilando...${NC}"
        make
        if [ $? -ne 0 ]; then
            echo -e "${RED}✗ Error en la compilación${NC}"
            exit 1
        fi
    fi
    
    echo -e "${GREEN}✓ Keylogger compilado y listo${NC}"
}

# Mostrar información del sistema
show_system_info() {
    echo -e "\n${YELLOW}[3/5] Información del sistema:${NC}"
    echo -e "  Display: $DISPLAY"
    echo -e "  Sesión: $XDG_SESSION_TYPE"
    echo -e "  Desktop: $XDG_CURRENT_DESKTOP"
    echo -e "  Usuario: $USER"
}

# Demostración de la vulnerabilidad
demonstrate_vulnerability() {
    echo -e "\n${YELLOW}[4/5] Demostración de Vulnerabilidad X11${NC}"
    echo -e "${BLUE}"
    cat << 'EOF'

X11 permite que CUALQUIER aplicación ejecute código como:

    Display *display = XOpenDisplay(NULL);
    Window root = DefaultRootWindow(display);
    XGrabKeyboard(display, root, ...);

Sin requerir:
  • Privilegios de root
  • Permisos especiales del sistema
  • Confirmación del usuario
  • Notificación visible

Esto significa que una aplicación aparentemente inofensiva
puede capturar TODAS tus contraseñas, mensajes y datos sensibles
sin que lo sepas.

EOF
    echo -e "${NC}"
}

# Ejecutar el keylogger con instrucciones
run_keylogger() {
    echo -e "\n${YELLOW}[5/5] Iniciando keylogger educativo...${NC}"
    echo -e "${GREEN}"
    cat << 'EOF'

DEMOSTRACIÓN:
1. El keylogger capturará todos los eventos de teclado
2. Abre cualquier aplicación (navegador, editor de texto, terminal)
3. Escribe algo y observa cómo se captura en tiempo real
4. Los eventos se guardan en 'keylog.txt'
5. Presiona Ctrl+C para detener

EOF
    echo -e "${NC}"
    
    echo -e "${RED}Presiona ENTER para iniciar la demostración...${NC}"
    read
    
    # Ejecutar el keylogger
    ./x11_keylogger
    
    # Después de detener
    echo -e "\n${YELLOW}Keylogger detenido${NC}"
    
    if [ -f "keylog.txt" ]; then
        echo -e "${GREEN}✓ Log guardado en: keylog.txt${NC}"
        echo -e "${YELLOW}Mostrando últimas 20 líneas capturadas:${NC}"
        echo -e "${BLUE}─────────────────────────────────────────${NC}"
        tail -20 keylog.txt
        echo -e "${BLUE}─────────────────────────────────────────${NC}"
    fi
}

# Mostrar recomendaciones de seguridad
show_security_recommendations() {
    echo -e "\n${YELLOW}═══════════════════════════════════════════════════════${NC}"
    echo -e "${YELLOW}  Recomendaciones de Seguridad${NC}"
    echo -e "${YELLOW}═══════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}"
    cat << 'EOF'

¿Cómo protegerte?

1. MIGRAR A WAYLAND (Más efectivo)
   - Wayland tiene seguridad moderna
   - Aislamiento entre aplicaciones
   - Permisos granulares

2. DETECTAR KEYLOGGERS
   $ ps aux | grep -i key
   $ lsof | grep X11

3. BUENAS PRÁCTICAS
   - Solo instalar software de fuentes confiables
   - Revisar procesos en ejecución regularmente
   - Usar gestores de contraseñas
   - Mantener el sistema actualizado

4. VERIFICAR TU SISTEMA
   $ echo $XDG_SESSION_TYPE
   
   Si retorna "wayland" → SEGURO
   Si retorna "x11" → VULNERABLE

EOF
    echo -e "${NC}"
}

# Limpieza
cleanup() {
    echo -e "\n${YELLOW}Limpieza...${NC}"
    echo -e "${BLUE}¿Deseas eliminar el archivo de log? (s/n)${NC}"
    read -r response
    if [[ "$response" =~ ^[Ss]$ ]]; then
        rm -f keylog.txt
        echo -e "${GREEN}✓ Log eliminado${NC}"
    else
        echo -e "${YELLOW}Log conservado en: keylog.txt${NC}"
    fi
}

# Función principal
main() {
    check_x11
    check_compilation
    show_system_info
    demonstrate_vulnerability
    run_keylogger
    show_security_recommendations
    cleanup
    
    echo -e "\n${GREEN}═══════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}  Demostración completada${NC}"
    echo -e "${GREEN}═══════════════════════════════════════════════════════${NC}"
    echo -e "\n${YELLOW}Recuerda: Este conocimiento es para EDUCACIÓN y DEFENSA${NC}"
    echo -e "${YELLOW}Usa tu conocimiento de forma ética y legal.${NC}\n"
}

# Capturar Ctrl+C para limpieza
trap cleanup EXIT

# Ejecutar
main
