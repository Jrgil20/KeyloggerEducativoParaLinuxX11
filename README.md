# KeyloggerEducativoParaLinuxX11

🔒 **Keylogger educativo** para sistemas Linux que utilizan el protocolo X11, diseñado para demostrar las vulnerabilidades inherentes del servidor gráfico.

## ⚠️ ADVERTENCIA LEGAL Y ÉTICA

**Este software es EXCLUSIVAMENTE para propósitos educativos y de investigación en seguridad informática.**

- ❌ **NO** usar en sistemas ajenos sin autorización explícita
- ❌ **NO** utilizar para actividades ilegales
- ❌ **NO** distribuir con intenciones maliciosas
- ✅ **SÍ** usar para aprendizaje y concienciación de seguridad
- ✅ **SÍ** usar en entornos controlados y propios

El uso no autorizado de keyloggers es **ilegal** en la mayoría de jurisdicciones y puede resultar en consecuencias legales graves.

---

## 📚 Descripción

Este proyecto demuestra cómo X11, diseñado en 1984, presenta carencias fundamentales de seguridad que permiten a **cualquier aplicación** capturar eventos de teclado de otras aplicaciones **sin necesidad de privilegios elevados**.

### ¿Qué demuestra este proyecto?

- **Vulnerabilidad de diseño**: X11 no implementa aislamiento entre aplicaciones
- **Falta de permisos**: No se requiere root para capturar eventos de teclado
- **Sin notificaciones**: El usuario no es alertado sobre la captura
- **Riesgo real**: Cualquier aplicación puede convertirse en keylogger

### Objetivo Educativo

1. **Concienciación**: Mostrar los riesgos de usar sistemas con X11
2. **Investigación**: Comprender vectores de ataque en entornos legacy
3. **Promoción de seguridad**: Motivar la migración a alternativas modernas como Wayland

---

## 🔧 Características

- ✅ Captura global de eventos de teclado en X11
- ✅ Identificación de la ventana/aplicación activa
- ✅ Registro de eventos con timestamp
- ✅ Conversión de keycodes a strings legibles
- ✅ Salida en archivo y consola
- ✅ Manejo de señales para limpieza segura
- ✅ Código comentado y documentado
- ✅ Sin dependencias externas complejas

---

## 🚀 Instalación

### Requisitos Previos

```bash
# En sistemas basados en Debian/Ubuntu
sudo apt-get update
sudo apt-get install build-essential libx11-dev

# En sistemas basados en Fedora/RHEL
sudo dnf install gcc libX11-devel

# En Arch Linux
sudo pacman -S base-devel libx11
```

### Compilación

```bash
# Clonar el repositorio
git clone https://github.com/Jrgil20/KeyloggerEducativoParaLinuxX11.git
cd KeyloggerEducativoParaLinuxX11

# Compilar
make

# Ver opciones disponibles
make help
```

---

## 📖 Uso

```bash
# Ejecutar el keylogger
./x11_keylogger
```

El programa:

1. Mostrará advertencias legales
2. Solicitará confirmación (escribir 's' para continuar)
3. Comenzará a capturar eventos de teclado
4. Guardará los eventos en `keylog.txt`
5. Mostrará eventos en tiempo real en la consola

**Para detener**: Presione `Ctrl+C`

### Ejemplo de Salida

``` bash
[2025-11-02 10:30:45] [Firefox - Mozilla] password
[2025-11-02 10:30:48] [Firefox - Mozilla] [ENTER]
[2025-11-02 10:30:50] [Terminal - bash] ls -la
[2025-11-02 10:30:51] [Terminal - bash] [ENTER]
```

---

## 🔬 ¿Cómo Funciona?

### Vulnerabilidad de X11

X11 permite que cualquier aplicación ejecute:

```c
Display *display = XOpenDisplay(NULL);
Window root = DefaultRootWindow(display);
XSelectInput(display, root, KeyPressMask);
XGrabKeyboard(display, root, True, GrabModeAsync, GrabModeAsync, CurrentTime);
```

**Sin requerir**:

- ❌ Privilegios de root
- ❌ Permisos especiales
- ❌ Autorización del usuario
- ❌ Notificación al sistema

### Arquitectura

1. **Conexión**: Se conecta al servidor X11 local
2. **Registro**: Se suscribe a eventos de teclado globales
3. **Captura**: Intercepta todas las teclas presionadas
4. **Procesamiento**: Identifica la ventana activa y la tecla presionada
5. **Logging**: Registra con timestamp en archivo y consola

---

## 🛡️ Protección y Mitigación

### ¿Estás vulnerable?

```bash
# Verificar si usas X11 o Wayland
echo $XDG_SESSION_TYPE

# Si retorna "x11" → VULNERABLE
# Si retorna "wayland" → PROTEGIDO
```

### Soluciones

1. **Migrar a Wayland** (Recomendado)
   - Seguridad moderna por diseño
   - Aislamiento entre aplicaciones
   - Permisos granulares

2. **Detectar keyloggers** (ver sección detallada más abajo)

3. **Buenas prácticas**
   - Solo ejecutar aplicaciones de fuentes confiables
   - Auditar regularmente procesos en ejecución
   - Usar herramientas de detección de malware
   - Mantener el sistema actualizado

---

## 🔍 Cómo Detectar Este Keylogger

Esta sección es **educativa** y demuestra que aunque el keylogger puede ocultarse de usuarios casuales, **siempre es detectable** por administradores o herramientas de seguridad.

### Nivel de Ocultamiento

| Método | Usuario Casual | Admin/Seguridad |
|--------|:--------------:|:---------------:|
| `ps aux` | ⚠️ Camuflado como `kworker/0:0` | ✅ Detectable |
| `top`/`htop` | ⚠️ Nombre camuflado | ✅ Detectable |
| `/proc` | N/A | ✅ Completamente expuesto |
| `lsof` | N/A | ✅ Muestra archivos abiertos |
| Antivirus | N/A | ✅ Detecta comportamiento |

### Métodos de Detección

#### 1. Buscar procesos falsos de kernel

El keylogger en modo daemon usa el nombre `kworker/0:0`. Los procesos reales del kernel tienen características específicas:

```bash
# Los kworker REALES tienen PPID = 2 (kthreadd)
ps -eo pid,ppid,comm | grep kworker

# Si ves un kworker con PPID != 2, es FALSO
# Ejemplo de salida sospechosa:
#   12345  1234  kworker/0:0   ← FALSO (PPID no es 2)
#   15     2     kworker/0:0   ← REAL  (PPID es 2)
```

#### 2. Verificar el binario real en /proc

```bash
# Encontrar PIDs de procesos llamados kworker
for pid in $(pgrep -f "kworker/0:0"); do
    echo "=== PID: $pid ==="
    # Ver el ejecutable real
    ls -la /proc/$pid/exe 2>/dev/null
    # Ver la línea de comandos original
    cat /proc/$pid/cmdline 2>/dev/null; echo
done

# Un proceso REAL del kernel mostrará:
#   /proc/15/exe -> (ningún enlace, error)
# 
# Este keylogger mostrará:
#   /proc/12345/exe -> /home/user/x11_keylogger   ← ¡EXPUESTO!
```

#### 3. Buscar conexiones a X11

```bash
# Listar procesos con conexiones al servidor X11
lsof -i -P | grep -E ":(6000|X11)"

# O buscar sockets Unix de X11
lsof | grep /tmp/.X11-unix

# El keylogger aparecerá con DOS conexiones a X11
# (una para eventos, otra para grabación)
```

#### 4. Buscar archivos de log abiertos

```bash
# Buscar procesos que tengan abierto keylog.txt
lsof | grep keylog

# O buscar cualquier archivo .txt sospechoso
lsof +D /tmp 2>/dev/null | grep -E "\.txt|\.log"
```

#### 5. Analizar el comportamiento con strace

```bash
# Adjuntar a un proceso sospechoso
sudo strace -p <PID> -e read,write

# Si es un keylogger, verás:
# - Lecturas constantes del socket X11
# - Escrituras al archivo de log
```

#### 6. Script de detección automática

```bash
#!/bin/bash
# detector_keylogger.sh - Detecta keyloggers X11 sospechosos

echo "=== Detector de Keyloggers X11 ==="
echo ""

# Buscar kworkers falsos
echo "[1] Buscando procesos kworker sospechosos..."
for pid in $(pgrep -f "kworker"); do
    ppid=$(ps -o ppid= -p $pid 2>/dev/null | tr -d ' ')
    if [ "$ppid" != "2" ] && [ -n "$ppid" ]; then
        exe=$(readlink /proc/$pid/exe 2>/dev/null)
        echo "  ⚠️  SOSPECHOSO: PID=$pid, PPID=$ppid, EXE=$exe"
    fi
done

# Buscar procesos con múltiples conexiones X11
echo ""
echo "[2] Procesos con conexiones X11 (sin terminal)..."
lsof 2>/dev/null | grep X11-unix | awk '{print $2}' | sort | uniq -c | \
    while read count pid; do
        if [ "$count" -ge 2 ]; then
            comm=$(ps -o comm= -p $pid 2>/dev/null)
            echo "  ⚠️  PID=$pid tiene $count conexiones X11 ($comm)"
        fi
    done

# Buscar archivos de log típicos
echo ""
echo "[3] Archivos de log sospechosos abiertos..."
lsof 2>/dev/null | grep -iE "keylog|keys\.txt|log\.txt" | head -5

echo ""
echo "=== Fin del análisis ==="
```

### Herramientas de Detección Recomendadas

| Herramienta | Uso | Comando |
|-------------|-----|---------|
| **rkhunter** | Detectar rootkits | `sudo rkhunter --check` |
| **chkrootkit** | Escaneo de sistema | `sudo chkrootkit` |
| **ClamAV** | Antivirus | `clamscan -r /home` |
| **auditd** | Auditoría de syscalls | `ausearch -k keylogger` |
| **ps/proc** | Análisis manual | Ver comandos arriba |

### Para Detener el Keylogger

```bash
# Encontrar el PID real
pid=$(for p in $(pgrep -f "kworker/0:0"); do
    ppid=$(ps -o ppid= -p $p | tr -d ' ')
    [ "$ppid" != "2" ] && echo $p
done)

# Terminar el proceso
kill $pid

# O forzar terminación
kill -9 $pid
```

---

## 📂 Estructura del Proyecto

``` bash
KeyloggerEducativoParaLinuxX11/
├── README.md                 # Este archivo
├── DOCUMENTACION.md          # Documentación técnica detallada
├── x11_keylogger.c          # Código fuente principal
├── Makefile                 # Sistema de compilación
└── keylog.txt              # Archivo de log (generado en ejecución)
```

---

## 📊 Comparación de Seguridad

| Sistema/Protocolo | Aislamiento | Permisos Requeridos | Notificación Usuario |
| :-: | :-: | :-: | :-: |
| **X11** | ❌ No | ❌ Ninguno | ❌ No |
| **Wayland** | ✅ Sí | ✅ Explícitos | ✅ Sí |
| **Windows** | ⚠️ Parcial | ✅ Admin (UAC) | ⚠️ Limitada |
| **macOS** | ✅ Sí | ✅ Explícitos | ✅ Sí |

---

## 🎓 Recursos Educativos

- [Documentación Técnica Completa](docs/DOCUMENTACION.md)
- [X11 Protocol Specification](https://www.x.org/releases/current/doc/xproto/x11protocol.html)
- [Wayland Security Model](https://wayland.freedesktop.org/docs/html/ch04.html#sect-Wayland-Security)
- [OWASP - Input Validation](https://owasp.org/www-community/vulnerabilities/)

---

## 🤝 Contribuciones

Este es un proyecto educativo. Las contribuciones son bienvenidas para:

- Mejorar la documentación
- Añadir ejemplos de mitigación
- Corregir bugs
- Mejorar el código educativo

**Recordatorio**: Este proyecto es para educación, no para desarrollo de malware.

---

## 📜 Licencia

Este proyecto se distribuye bajo licencia MIT con las siguientes condiciones adicionales:

- Uso exclusivamente educativo y de investigación
- Prohibido el uso con fines maliciosos o ilegales
- El autor no se hace responsable del mal uso
- Cumplir con las leyes locales sobre seguridad informática

---

## 👤 Autors

Jrgil20
- GitHub: [@Jrgil20](https://github.com/Jrgil20)

Co-authored-by: David E. Hidalgo V. <David-Hidalgo@users.noreply.github.com>

---

## ⚖️ Responsabilidad

El autor de este proyecto:

- ✅ Proporciona este código con fines educativos
- ✅ Advierte sobre las implicaciones legales
- ✅ Promueve el uso ético y responsable
- ❌ NO se responsabiliza por el mal uso
- ❌ NO apoya actividades ilegales

**Recuerda**: Con gran poder viene gran responsabilidad. Usa este conocimiento para mejorar la seguridad, no para vulnerarla.

---

## 🔗 Enlaces Relacionados

- [Migrar a Wayland](https://wiki.archlinux.org/title/Wayland)
- [Seguridad en Linux](https://www.kernel.org/doc/html/latest/security/)
- [Ethical Hacking](https://www.eccouncil.org/ethical-hacking/)

---

**🔐 Stay Safe. Stay Ethical. Stay Legal.**
