# Documentación Técnica: X11 Educational Keylogger

## Índice
1. [Introducción](#introducción)
2. [Vulnerabilidades de X11](#vulnerabilidades-de-x11)
3. [Arquitectura del Programa](#arquitectura-del-programa)
4. [Instalación y Uso](#instalación-y-uso)
5. [Análisis de Seguridad](#análisis-de-seguridad)
6. [Mitigaciones](#mitigaciones)
7. [Referencias](#referencias)

## Introducción

Este keylogger educativo demuestra las vulnerabilidades inherentes del protocolo X11, un servidor gráfico diseñado en 1984 para sistemas Unix y Linux. El objetivo es **educativo**: mostrar cómo las aplicaciones pueden capturar eventos de teclado sin privilegios elevados.

### Propósito Educativo

- **Concienciación de seguridad**: Comprender los riesgos de usar sistemas con X11
- **Investigación**: Analizar vectores de ataque en entornos legacy
- **Demostración técnica**: Mostrar limitaciones del diseño de X11
- **Promoción de alternativas**: Motivar la migración a Wayland

## Vulnerabilidades de X11

### 1. Falta de Aislamiento entre Aplicaciones

X11 fue diseñado cuando la seguridad no era una prioridad. El protocolo permite que **cualquier aplicación** acceda a eventos de otras aplicaciones:

```c
// Cualquier aplicación puede ejecutar esto sin privilegios especiales
Display *display = XOpenDisplay(NULL);
Window root = DefaultRootWindow(display);
XSelectInput(display, root, KeyPressMask);
XGrabKeyboard(display, root, True, GrabModeAsync, GrabModeAsync, CurrentTime);
```

### 2. Captura Global de Eventos

El servidor X11 no implementa:
- **Sandboxing**: Las aplicaciones no están aisladas
- **Control de acceso granular**: No hay permisos por aplicación
- **Notificaciones**: El usuario no sabe cuando se capturan teclas
- **Validación de privilegios**: No se requiere elevación para operaciones sensibles

### 3. Modelo de Seguridad Obsoleto

Diseñado en una época donde:
- Los sistemas eran monousuario
- Las aplicaciones eran confiables
- No existía el concepto de malware moderno
- La red no era ubicua

### 4. Vectores de Ataque Demostrados

Este keylogger demuestra:

1. **Captura de credenciales**: Contraseñas escritas en cualquier aplicación
2. **Espionaje de comunicaciones**: Mensajes de chat, emails
3. **Robo de información sensible**: Datos bancarios, información personal
4. **Persistencia**: Puede ejecutarse en segundo plano sin detección

## Arquitectura del Programa

### Componentes Principales

```
┌─────────────────────────────────────┐
│      X11 Display Server             │
│  (Gestor de ventanas y eventos)     │
└──────────────┬──────────────────────┘
               │
               │ XEvent Stream
               │
┌──────────────▼──────────────────────┐
│     x11_keylogger.c                 │
│                                     │
│  ┌───────────────────────────────┐ │
│  │  Event Capture Loop           │ │
│  │  - XSelectInput()             │ │
│  │  - XGrabKeyboard()            │ │
│  │  - XNextEvent()               │ │
│  └───────────┬───────────────────┘ │
│              │                      │
│  ┌───────────▼───────────────────┐ │
│  │  Key Processing               │ │
│  │  - XLookupKeysym()            │ │
│  │  - keysym_to_string()         │ │
│  │  - get_window_name()          │ │
│  └───────────┬───────────────────┘ │
│              │                      │
│  ┌───────────▼───────────────────┐ │
│  │  Logging Module               │ │
│  │  - File output (keylog.txt)   │ │
│  │  - Console output              │ │
│  │  - Timestamp generation        │ │
│  └───────────────────────────────┘ │
└─────────────────────────────────────┘
```

### Flujo de Ejecución

1. **Inicialización**
   - Conectar al servidor X11 (`XOpenDisplay`)
   - Obtener ventana raíz
   - Configurar manejadores de señales

2. **Registro de Eventos**
   - Seleccionar eventos de teclado (`XSelectInput`)
   - Capturar teclado globalmente (`XGrabKeyboard`)

3. **Procesamiento**
   - Esperar eventos (`XNextEvent`)
   - Identificar ventana activa
   - Convertir keycode → keysym → string
   - Registrar con timestamp

4. **Limpieza**
   - Liberar recursos (`XUngrabKeyboard`)
   - Cerrar conexión X11
   - Cerrar archivos de log

### Funciones Clave

#### `XGrabKeyboard()`
```c
XGrabKeyboard(display, root, True, GrabModeAsync, GrabModeAsync, CurrentTime);
```
**Vulnerabilidad crítica**: Permite capturar TODAS las teclas del sistema sin restricciones.

#### `XSelectInput()`
```c
XSelectInput(display, root, KeyPressMask | KeyReleaseMask);
```
**Problema**: Cualquier aplicación puede suscribirse a eventos de teclado globales.

#### `get_focused_window()`
```c
Window get_focused_window(Display *display) {
    Window focused;
    int revert;
    XGetInputFocus(display, &focused, &revert);
    return focused;
}
```
**Riesgo de privacidad**: Permite identificar qué aplicación está usando el usuario.

## Instalación y Uso

### Requisitos

- Sistema Linux con X11 (no Wayland)
- GCC compiler
- Librerías de desarrollo X11

```bash
# En distribuciones basadas en Debian/Ubuntu
sudo apt-get install build-essential libx11-dev

# En distribuciones basadas en Fedora/RHEL
sudo dnf install gcc libX11-devel

# En Arch Linux
sudo pacman -S base-devel libx11
```

### Compilación

```bash
# Clonar repositorio
git clone https://github.com/Jrgil20/KeyloggerEducativoParaLinuxX11.git
cd KeyloggerEducativoParaLinuxX11

# Compilar
make

# Ver ayuda
make help
```

### Ejecución

```bash
# Ejecutar el keylogger
./x11_keylogger

# El programa solicitará confirmación
# Escriba 's' para continuar

# Detener con Ctrl+C
```

### Salida del Programa

El keylogger genera:
1. **Salida en consola**: Eventos en tiempo real
2. **Archivo `keylog.txt`**: Registro persistente con timestamps

Ejemplo de salida:
```
[2025-11-02 10:30:45] [Firefox - Mozilla] p
[2025-11-02 10:30:45] [Firefox - Mozilla] a
[2025-11-02 10:30:46] [Firefox - Mozilla] s
[2025-11-02 10:30:46] [Firefox - Mozilla] s
[2025-11-02 10:30:47] [Firefox - Mozilla] w
[2025-11-02 10:30:47] [Firefox - Mozilla] o
[2025-11-02 10:30:47] [Firefox - Mozilla] r
[2025-11-02 10:30:48] [Firefox - Mozilla] d
[2025-11-02 10:30:48] [Firefox - Mozilla] [ENTER]
```

## Análisis de Seguridad

### ¿Por qué funciona sin root?

X11 no requiere privilegios elevados para:
- Conectarse al servidor (`XOpenDisplay`)
- Capturar eventos de teclado (`XGrabKeyboard`)
- Leer nombres de ventanas (`XFetchName`)

Esto es una **decisión de diseño**, no un bug.

### Comparación con Sistemas Modernos

| Sistema | Aislamiento | Permisos | Notificación Usuario |
|---------|-------------|----------|---------------------|
| X11 | ❌ No | ❌ No requeridos | ❌ No |
| Wayland | ✅ Sí | ✅ Por aplicación | ✅ Sí |
| Windows | ⚠️ Parcial | ✅ UAC/Admin | ⚠️ Limitada |
| macOS | ✅ Sí | ✅ Permisos explícitos | ✅ Sí |

### Escenarios de Ataque Reales

1. **Malware en software legítimo**
   - Aplicación aparentemente inofensiva incluye keylogger
   - Usuario la ejecuta voluntariamente
   - Captura credenciales sin detección

2. **Persistencia**
   - Se agrega a scripts de inicio (`~/.bashrc`, `~/.xinitrc`)
   - Ejecuta en segundo plano
   - Difícil de detectar sin herramientas especializadas

3. **Ataque de ingeniería social**
   - "Instalar este programa útil"
   - Usuario ejecuta sin sospechar
   - Keylogger captura información sensible

## Mitigaciones

### 1. Migrar a Wayland

**Solución recomendada**: Wayland implementa seguridad moderna.

```bash
# Verificar si estás usando Wayland o X11
echo $XDG_SESSION_TYPE

# Si retorna "x11", estás vulnerable
# Si retorna "wayland", estás protegido
```

### 2. Usar Herramientas de Monitoreo

```bash
# Detectar programas capturando eventos
ps aux | grep -i key

# Monitorear conexiones X11
xdpyinfo -ext XTEST

# Revisar aplicaciones con acceso a input
lsof | grep X11
```

### 3. Políticas de Seguridad

- **Principio de mínimo privilegio**: Solo ejecutar aplicaciones confiables
- **Verificación de integridad**: Comprobar checksums de software
- **Auditoría regular**: Revisar procesos en ejecución
- **Firewall de aplicaciones**: Limitar acceso de red

### 4. Detección de Keyloggers

**Señales de advertencia**:
- Procesos desconocidos con X11 en el nombre
- Alto uso de CPU sin razón aparente
- Archivos de log sospechosos
- Conexiones de red no autorizadas

**Herramientas de detección**:
```bash
# Listar procesos con X11
ps aux | grep X

# Verificar archivos abiertos por procesos
lsof -u $USER | grep -i log

# Monitorear llamadas al sistema
strace -e trace=open,openat -p [PID]
```

### 5. Configuración Segura de X11

**Limitar acceso con xhost**:
```bash
# Denegar acceso a todas las máquinas
xhost -

# Permitir solo conexiones locales
xhost +local:
```

**Usar Xauthority**:
```bash
# Verificar archivo de autorización
echo $XAUTHORITY
# Típicamente: /home/usuario/.Xauthority
```

### 6. Alternativas Modernas

| Alternativa | Seguridad | Compatibilidad | Rendimiento |
|-------------|-----------|----------------|-------------|
| Wayland | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ |
| Wayland + XWayland | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| X11 con políticas | ⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ |

## Referencias

### Documentación Técnica

1. **X11 Protocol Specification**
   - https://www.x.org/releases/current/doc/xproto/x11protocol.html

2. **Xlib Programming Manual**
   - https://www.x.org/releases/X11R7.7/doc/libX11/libX11/libX11.html

3. **Wayland Security**
   - https://wayland.freedesktop.org/docs/html/ch04.html#sect-Wayland-Security

### Artículos de Seguridad

1. **"X11 Security Model"** - X.Org Foundation
2. **"Input Isolation in Wayland"** - Collabora
3. **"X Window System Security"** - OWASP

### Herramientas Relacionadas

- **xinput**: Gestionar dispositivos de entrada X11
- **xev**: Monitorear eventos X11
- **xdotool**: Simular eventos de teclado/mouse
- **rkhunter**: Detectar rootkits y keyloggers

### Papers Académicos

1. Shapiro, J. et al. (2003). "Security Holes in X Window System"
2. Kilpatrick, D. (2003). "Privman: A Library for Partitioning Applications"

## Conclusión

Este keylogger educativo demuestra que:

1. ✅ X11 tiene vulnerabilidades fundamentales de diseño
2. ✅ La captura de teclas no requiere privilegios especiales
3. ✅ No hay protecciones nativas en X11
4. ✅ Los usuarios están expuestos sin saberlo
5. ✅ La migración a Wayland es necesaria

### Recomendaciones Finales

- **Para usuarios**: Migrar a distribuciones con Wayland por defecto
- **Para administradores**: Auditar regularmente sistemas con X11
- **Para desarrolladores**: Considerar alternativas seguras
- **Para todos**: Concienciación sobre riesgos de seguridad

---

**AVISO LEGAL**: Este documento y el código asociado son solo para propósitos educativos. El uso no autorizado de keyloggers es ilegal en la mayoría de las jurisdicciones. Use esta información responsablemente y solo en sistemas propios o con autorización explícita.
