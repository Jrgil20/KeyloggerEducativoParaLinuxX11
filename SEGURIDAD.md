# Guía de Seguridad: Protección contra Keyloggers en X11

## Índice
1. [Introducción](#introducción)
2. [¿Cómo detectar si eres vulnerable?](#cómo-detectar-si-eres-vulnerable)
3. [Detección de keyloggers activos](#detección-de-keyloggers-activos)
4. [Métodos de protección](#métodos-de-protección)
5. [Migración a Wayland](#migración-a-wayland)
6. [Herramientas de seguridad](#herramientas-de-seguridad)
7. [Mejores prácticas](#mejores-prácticas)

---

## Introducción

Esta guía proporciona información práctica sobre cómo protegerse de keyloggers que explotan vulnerabilidades de X11. Si estás usando X11, **estás potencialmente vulnerable** a este tipo de ataques.

### Nivel de Riesgo

| Escenario | Riesgo | Descripción |
|-----------|--------|-------------|
| X11 + Software no confiable | 🔴 CRÍTICO | Máxima vulnerabilidad |
| X11 + Solo software confiable | 🟡 MEDIO | Vulnerable pero controlado |
| Wayland | 🟢 BAJO | Protegido por diseño |
| Wayland + Buenas prácticas | 🟢 MUY BAJO | Máxima seguridad |

---

## ¿Cómo detectar si eres vulnerable?

### Verificar tu sistema gráfico

```bash
# Método 1: Variable de entorno
echo $XDG_SESSION_TYPE

# Método 2: Verificar proceso
ps aux | grep -i wayland

# Método 3: loginctl
loginctl show-session $(loginctl | grep $(whoami) | awk '{print $1}') -p Type
```

**Interpretación de resultados:**
- `x11` → ❌ VULNERABLE a este ataque
- `wayland` → ✅ PROTEGIDO
- `tty` → N/A (sin interfaz gráfica)

### Verificar si X11 está corriendo

```bash
# Verificar display
echo $DISPLAY

# Verificar servidor X
ps aux | grep Xorg

# Información del display
xdpyinfo | head -20
```

---

## Detección de keyloggers activos

### 1. Monitoreo de procesos

```bash
# Buscar procesos sospechosos relacionados con X11
ps aux | grep -E "(X11|xkey|keylog|xinput)" | grep -v grep

# Listar todos los procesos con conexión X11
lsof -U | grep X11

# Ver procesos por uso de CPU (keyloggers consumen recursos)
top -o %CPU
```

### 2. Verificar archivos de log sospechosos

```bash
# Buscar archivos de log recientes en home
find ~ -type f -name "*log*" -mtime -1

# Buscar archivos modificados recientemente
find ~ -type f -mmin -60 -ls

# Buscar archivos ocultos sospechosos
find ~ -type f -name ".*" -mtime -7
```

### 3. Monitorear conexiones de red

```bash
# Ver conexiones de red activas
netstat -tunap | grep ESTABLISHED

# Usar ss (más moderno)
ss -tunap | grep ESTABLISHED

# Verificar conexiones por aplicación
lsof -i -n -P | grep -v LISTEN
```

### 4. Auditar llamadas al sistema

```bash
# Capturar llamadas a sistema de un proceso específico
strace -e trace=open,openat,write -p [PID]

# Ver todas las llamadas X11 de un proceso
strace -e trace=network -p [PID] 2>&1 | grep X11
```

### 5. Usar herramientas especializadas

```bash
# Instalar rkhunter (Rootkit Hunter)
sudo apt-get install rkhunter
sudo rkhunter --check

# Instalar chkrootkit
sudo apt-get install chkrootkit
sudo chkrootkit

# Usar OSSEC para detección de intrusos
# https://www.ossec.net/
```

---

## Métodos de protección

### Protección Nivel 1: Inmediata (X11)

Medidas que puedes tomar ahora mismo sin cambiar de sistema:

#### A. Limitar acceso X11

```bash
# Denegar acceso a todas las máquinas
xhost -

# Permitir solo conexiones locales
xhost +local:

# Verificar lista de control de acceso
xhost
```

#### B. Usar Xauthority correctamente

```bash
# Verificar archivo de autorización
ls -la ~/.Xauthority

# Regenerar si es necesario
xauth generate $DISPLAY . trusted

# Ver cookies de autorización
xauth list
```

#### C. Monitoreo activo

Crear script de monitoreo (`~/monitor_x11.sh`):

```bash
#!/bin/bash
# Script de monitoreo para detectar procesos sospechosos

LOG_FILE="$HOME/x11_monitor.log"

while true; do
    # Buscar procesos con XGrabKeyboard
    SUSPICIOUS=$(ps aux | grep -E "(keylog|X11|grab)" | grep -v grep | grep -v monitor)
    
    if [ ! -z "$SUSPICIOUS" ]; then
        echo "[$(date)] ALERTA: Proceso sospechoso detectado" >> $LOG_FILE
        echo "$SUSPICIOUS" >> $LOG_FILE
        # Enviar notificación
        notify-send "ALERTA DE SEGURIDAD" "Proceso sospechoso detectado"
    fi
    
    sleep 60  # Verificar cada minuto
done
```

Ejecutar en segundo plano:
```bash
chmod +x ~/monitor_x11.sh
nohup ~/monitor_x11.sh &
```

#### D. Firewall de aplicaciones

```bash
# Instalar y configurar AppArmor (Ubuntu/Debian)
sudo apt-get install apparmor apparmor-utils

# O instalar SELinux (Fedora/RHEL)
sudo dnf install selinux-policy selinux-policy-targeted
```

### Protección Nivel 2: Avanzada (X11)

#### A. Sandboxing con Firejail

```bash
# Instalar Firejail
sudo apt-get install firejail

# Ejecutar aplicaciones en sandbox
firejail firefox
firejail --private thunderbird

# Crear perfil personalizado para aplicaciones
sudo firejail --profile=/etc/firejail/firefox.profile firefox
```

#### B. Control de acceso con PolicyKit

```bash
# Instalar PolicyKit
sudo apt-get install policykit-1

# Configurar políticas restrictivas
sudo nano /etc/polkit-1/localauthority/50-local.d/restrict-x11.pkla
```

Agregar:
```ini
[Restrict X11 Access]
Identity=unix-user:*
Action=org.freedesktop.x11.*
ResultAny=no
ResultInactive=no
ResultActive=auth_admin
```

#### C. Usar Xephyr (X11 anidado)

```bash
# Instalar Xephyr
sudo apt-get install xserver-xephyr

# Ejecutar aplicaciones no confiables en X11 separado
Xephyr -screen 1024x768 :2 &
DISPLAY=:2 untrusted-app
```

### Protección Nivel 3: Máxima (Wayland)

La mejor protección es migrar a Wayland. Ver sección siguiente.

---

## Migración a Wayland

### Ventajas de Wayland

✅ **Seguridad por diseño**: Aislamiento completo entre aplicaciones
✅ **Sin captura global**: Las apps no pueden capturar eventos de otras
✅ **Permisos explícitos**: El usuario autoriza cada acceso
✅ **Mejor rendimiento**: Menos overhead que X11
✅ **Desarrollo activo**: Futuro de Linux desktop

### Verificar compatibilidad

```bash
# Verificar si Wayland está disponible
ls /usr/bin/*wayland* 2>/dev/null

# Verificar compositors disponibles
dpkg -l | grep -E "(wayland|weston|sway)"
```

### Migración en Ubuntu/Debian

```bash
# 1. Instalar sesión Wayland para GNOME
sudo apt-get install gnome-session-wayland

# 2. Cerrar sesión
# 3. En pantalla de login, hacer click en icono de configuración
# 4. Seleccionar "GNOME on Wayland"
# 5. Iniciar sesión

# Verificar que estás en Wayland
echo $XDG_SESSION_TYPE  # Debe mostrar "wayland"
```

### Migración en Fedora

```bash
# Wayland es predeterminado en Fedora 25+
# Si estás usando X11, cambiar en GDM

# Método 1: Editar configuración de GDM
sudo nano /etc/gdm/custom.conf

# Comentar esta línea si existe:
# WaylandEnable=false

# Método 2: Desde interfaz de login
# Seleccionar "GNOME" (sin "Xorg")
```

### Migración en Arch Linux

```bash
# Instalar compositor Wayland
sudo pacman -S wayland

# Para GNOME
sudo pacman -S gnome-session-wayland

# Para KDE Plasma
sudo pacman -S plasma-wayland-session

# Para Sway (i3 para Wayland)
sudo pacman -S sway
```

### Aplicaciones que requieren XWayland

Algunas aplicaciones aún requieren X11. Wayland incluye XWayland para compatibilidad:

```bash
# Verificar si una app usa XWayland
xeyes  # Si los ojos siguen el cursor, estás en X11/XWayland

# Listar ventanas XWayland
xlsclients -l
```

**Nota**: Las aplicaciones en XWayland siguen vulnerables, pero están aisladas de las aplicaciones Wayland nativas.

---

## Herramientas de seguridad

### 1. xinput (Monitoreo de dispositivos de entrada)

```bash
# Listar dispositivos de entrada
xinput list

# Ver propiedades de un dispositivo
xinput list-props [device-id]

# Deshabilitar dispositivo temporalmente
xinput disable [device-id]
```

### 2. xev (Monitor de eventos X11)

```bash
# Monitorear eventos en tiempo real
xev

# Filtrar solo eventos de teclado
xev | grep KeyPress
```

### 3. xdotool (Útil para detección)

```bash
# Instalar
sudo apt-get install xdotool

# Ver ventana activa
xdotool getactivewindow getwindowname

# Detectar capturas de teclado
xdotool search --name keylogger
```

### 4. Lynis (Auditoría de seguridad)

```bash
# Instalar
sudo apt-get install lynis

# Ejecutar auditoría completa
sudo lynis audit system

# Revisar resultados
cat /var/log/lynis.log
```

### 5. AIDE (Advanced Intrusion Detection Environment)

```bash
# Instalar
sudo apt-get install aide

# Inicializar base de datos
sudo aideinit

# Verificar integridad
sudo aide --check
```

---

## Mejores prácticas

### Para usuarios finales

1. **🔒 Usar solo software confiable**
   - Instalar solo desde repositorios oficiales
   - Verificar firmas GPG de paquetes
   - Evitar software de fuentes desconocidas

2. **👀 Monitorear el sistema regularmente**
   ```bash
   # Script diario de verificación
   #!/bin/bash
   echo "=== Reporte de Seguridad $(date) ===" > ~/security_report.txt
   ps aux | grep -E "(X11|key)" >> ~/security_report.txt
   netstat -tunap | grep ESTABLISHED >> ~/security_report.txt
   ```

3. **🔄 Mantener el sistema actualizado**
   ```bash
   # Ubuntu/Debian
   sudo apt update && sudo apt upgrade
   
   # Fedora
   sudo dnf update
   
   # Arch
   sudo pacman -Syu
   ```

4. **🔐 Usar gestores de contraseñas**
   - KeePassXC (offline)
   - Bitwarden (online)
   - pass (línea de comandos)

5. **🎯 Principio de mínimo privilegio**
   - No ejecutar aplicaciones como root innecesariamente
   - Usar `sudo` solo cuando sea necesario

### Para administradores de sistemas

1. **📊 Implementar monitoreo centralizado**
   - Syslog centralizado
   - SIEM (Security Information and Event Management)
   - Alertas automáticas

2. **🔍 Auditorías regulares**
   ```bash
   # Script de auditoría semanal
   #!/bin/bash
   rkhunter --check --skip-keypress
   chkrootkit
   lynis audit system --quick
   ```

3. **🛡️ Políticas de seguridad**
   - Deshabilitar X11 forwarding si no es necesario
   - Restringir acceso SSH
   - Implementar 2FA

4. **📚 Capacitación de usuarios**
   - Concientización sobre phishing
   - Buenas prácticas de seguridad
   - Reportar comportamientos sospechosos

5. **🔄 Plan de respuesta a incidentes**
   - Procedimientos de detección
   - Pasos de contención
   - Análisis forense
   - Recuperación

### Para desarrolladores

1. **🔐 Desarrollar para Wayland primero**
   - Usar APIs modernas
   - No asumir X11

2. **🔒 Implementar sandboxing**
   - Usar Flatpak o Snap
   - Solicitar solo permisos necesarios

3. **📝 Documentar requisitos de seguridad**
   - Especificar permisos necesarios
   - Advertir sobre riesgos

4. **🧪 Pruebas de seguridad**
   - Análisis estático de código
   - Pruebas de penetración
   - Code review enfocado en seguridad

---

## Checklist de seguridad rápida

### Verificación básica (5 minutos)

- [ ] Verificar tipo de sesión: `echo $XDG_SESSION_TYPE`
- [ ] Buscar procesos sospechosos: `ps aux | grep -i key`
- [ ] Verificar conexiones de red: `netstat -tunap`
- [ ] Revisar archivos de log recientes: `find ~ -name "*log*" -mtime -1`
- [ ] Actualizar sistema: `sudo apt update && sudo apt upgrade`

### Auditoría completa (30 minutos)

- [ ] Ejecutar rkhunter: `sudo rkhunter --check`
- [ ] Ejecutar chkrootkit: `sudo chkrootkit`
- [ ] Auditar con Lynis: `sudo lynis audit system`
- [ ] Verificar integridad con AIDE: `sudo aide --check`
- [ ] Revisar logs del sistema: `sudo journalctl -p err -b`
- [ ] Verificar usuarios conectados: `who` y `w`
- [ ] Revisar últimos logins: `last`
- [ ] Verificar archivos SUID: `find / -perm -4000 2>/dev/null`

---

## Recursos adicionales

### Documentación oficial

- [Wayland Documentation](https://wayland.freedesktop.org/docs/html/)
- [X.Org Security](https://www.x.org/wiki/Development/Security/)
- [Linux Security Modules](https://www.kernel.org/doc/html/latest/admin-guide/LSM/)

### Herramientas recomendadas

- [Firejail](https://firejail.wordpress.com/) - Sandboxing
- [AppArmor](https://apparmor.net/) - Mandatory Access Control
- [SELinux](https://selinuxproject.org/) - Security-Enhanced Linux
- [OSSEC](https://www.ossec.net/) - Host-based IDS

### Comunidades de seguridad

- [r/linux_security](https://reddit.com/r/linux_security)
- [r/netsec](https://reddit.com/r/netsec)
- [Linux Security Mailing Lists](https://www.kernel.org/doc/html/latest/process/security-bugs.html)

---

## Conclusión

La mejor defensa contra keyloggers en X11 es **migrar a Wayland**. Si no es posible inmediatamente:

1. ✅ Usa solo software confiable
2. ✅ Monitorea tu sistema regularmente
3. ✅ Mantén todo actualizado
4. ✅ Implementa capas de seguridad adicionales
5. ✅ Planifica la migración a Wayland

**Recuerda**: La seguridad es un proceso continuo, no un estado final.

---

**Última actualización**: 2025-11-02
**Versión**: 1.0
