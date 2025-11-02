# Guía de Instalación - X11 Educational Keylogger

## Requisitos del Sistema

### Sistema Operativo
- ✅ Linux con X11 (Ubuntu, Debian, Fedora, Arch, etc.)
- ❌ Windows (no compatible)
- ❌ macOS (no compatible)
- ❌ Linux con Wayland puro (no vulnerable, por lo que no funciona)

### Software Necesario
- GCC (GNU Compiler Collection) versión 4.8 o superior
- Make
- Librerías de desarrollo X11 (Xlib)
- Git (para clonar el repositorio)

---

## Instalación por Distribución

### Ubuntu / Debian / Linux Mint

```bash
# Actualizar repositorios
sudo apt update

# Instalar dependencias
sudo apt install -y build-essential libx11-dev git

# Verificar instalación
gcc --version
make --version
dpkg -l | grep libx11-dev
```

### Fedora / RHEL / CentOS

```bash
# Actualizar sistema
sudo dnf update

# Instalar dependencias
sudo dnf install -y gcc make libX11-devel git

# Verificar instalación
gcc --version
make --version
rpm -qa | grep libX11-devel
```

### Arch Linux / Manjaro

```bash
# Actualizar sistema
sudo pacman -Syu

# Instalar dependencias
sudo pacman -S base-devel libx11 git

# Verificar instalación
gcc --version
make --version
pacman -Q libx11
```

### openSUSE

```bash
# Actualizar sistema
sudo zypper refresh

# Instalar dependencias
sudo zypper install -y gcc make libX11-devel git

# Verificar instalación
gcc --version
make --version
rpm -qa | grep libX11
```

---

## Proceso de Instalación

### Paso 1: Clonar el Repositorio

```bash
# Clonar desde GitHub
git clone https://github.com/Jrgil20/KeyloggerEducativoParaLinuxX11.git

# Entrar al directorio
cd KeyloggerEducativoParaLinuxX11

# Verificar archivos
ls -la
```

**Archivos esperados:**
- `x11_keylogger.c` - Código fuente principal
- `Makefile` - Sistema de compilación
- `README.md` - Documentación principal
- `DOCUMENTACION.md` - Documentación técnica detallada
- `SEGURIDAD.md` - Guía de seguridad
- `demo.sh` - Script de demostración
- `LICENSE` - Licencia y términos de uso

### Paso 2: Compilar

```bash
# Compilar el keylogger
make

# Verificar compilación exitosa
ls -l x11_keylogger
```

**Salida esperada:**
```
Compilando x11_keylogger...
gcc -Wall -Wextra -O2 -o x11_keylogger x11_keylogger.c -lX11
✓ Compilación exitosa!
Ejecute './x11_keylogger' para iniciar el keylogger
```

### Paso 3: Verificar Sistema X11

```bash
# Verificar que estás usando X11
echo $XDG_SESSION_TYPE

# Verificar display
echo $DISPLAY

# Verificar servidor X
ps aux | grep Xorg
```

**Si retorna "wayland":** Este sistema no es vulnerable y el keylogger no funcionará (¡lo cual es bueno para seguridad!).

**Si retorna "x11":** El sistema es vulnerable y el keylogger funcionará.

### Paso 4: Ejecutar (SOLO EN ENTORNOS DE PRUEBA)

```bash
# Ejecutar el keylogger
./x11_keylogger

# Confirmar con 's' cuando se solicite
```

---

## Solución de Problemas

### Error: "No se puede conectar al servidor X11"

**Síntoma:**
```
[!] Error: No se puede conectar al servidor X11.
[!] Asegúrese de estar en un entorno con X11 activo.
```

**Solución:**
1. Verificar que estás en una sesión gráfica (no SSH sin X forwarding)
2. Verificar variable DISPLAY:
   ```bash
   echo $DISPLAY
   # Debe retornar algo como ":0" o ":1"
   ```
3. Verificar que X11 está corriendo:
   ```bash
   ps aux | grep Xorg
   ```

### Error: "X11/Xlib.h: No such file or directory"

**Síntoma:**
```
x11_keylogger.c:18:10: fatal error: X11/Xlib.h: No such file or directory
```

**Solución:**
Instalar librerías de desarrollo X11:

```bash
# Ubuntu/Debian
sudo apt install libx11-dev

# Fedora/RHEL
sudo dnf install libX11-devel

# Arch Linux
sudo pacman -S libx11
```

### Error: "gcc: command not found"

**Síntoma:**
```
make: gcc: Command not found
```

**Solución:**
Instalar compilador GCC:

```bash
# Ubuntu/Debian
sudo apt install build-essential

# Fedora/RHEL
sudo dnf install gcc make

# Arch Linux
sudo pacman -S base-devel
```

### Error: "Permission denied"

**Síntoma:**
```
bash: ./x11_keylogger: Permission denied
```

**Solución:**
Dar permisos de ejecución:
```bash
chmod +x x11_keylogger
```

### Error: Compilación con warnings

**Síntoma:**
```
warning: unused variable 'something'
warning: implicit declaration of function
```

**Solución:**
Los warnings son normales en desarrollo. Si la compilación termina exitosamente (exit code 0), el programa funciona. Para eliminar warnings, revisar el código.

---

## Instalación Avanzada

### Instalación Global (Opcional)

**⚠️ ADVERTENCIA:** Solo instalar globalmente si comprendes las implicaciones de seguridad.

```bash
# Compilar
make

# Instalar en /usr/local/bin (requiere sudo)
sudo make install

# Ahora puedes ejecutar desde cualquier lugar
x11_keylogger

# Para desinstalar
sudo make uninstall
```

### Compilación con Opciones Personalizadas

```bash
# Compilación con debugging
gcc -g -Wall -Wextra -o x11_keylogger x11_keylogger.c -lX11

# Compilación con optimización máxima
gcc -O3 -Wall -Wextra -o x11_keylogger x11_keylogger.c -lX11

# Compilación estática (incluir librerías)
gcc -static -Wall -Wextra -o x11_keylogger x11_keylogger.c -lX11 -lxcb -lXau -lXdmcp
```

### Verificación de Integridad

```bash
# Generar checksum del código fuente
sha256sum x11_keylogger.c

# Generar checksum del binario
sha256sum x11_keylogger

# Guardar checksums
sha256sum x11_keylogger.c x11_keylogger > checksums.txt

# Verificar más tarde
sha256sum -c checksums.txt
```

---

## Uso del Script de Demostración

### Ejecución Guiada

```bash
# Hacer ejecutable (si no lo está)
chmod +x demo.sh

# Ejecutar script de demostración
./demo.sh
```

El script:
1. ✅ Verifica que estés en X11
2. ✅ Compila si es necesario
3. ✅ Muestra información del sistema
4. ✅ Explica la vulnerabilidad
5. ✅ Ejecuta el keylogger con supervisión
6. ✅ Muestra recomendaciones de seguridad

---

## Desinstalación

### Desinstalación Local

```bash
# Entrar al directorio
cd KeyloggerEducativoParaLinuxX11

# Limpiar compilación
make clean

# Eliminar archivos de log
rm -f keylog.txt

# Salir del directorio
cd ..

# Eliminar directorio completo
rm -rf KeyloggerEducativoParaLinuxX11
```

### Desinstalación Global

```bash
# Si se instaló globalmente
sudo make uninstall

# O manualmente
sudo rm /usr/local/bin/x11_keylogger
```

---

## Entornos de Prueba

### Máquina Virtual (Recomendado)

**VirtualBox:**
```bash
# Instalar VirtualBox
sudo apt install virtualbox

# Crear VM con Ubuntu/Debian
# Instalar X11 (no Wayland)
# Probar keylogger en VM aislada
```

**KVM/QEMU:**
```bash
# Instalar KVM
sudo apt install qemu-kvm libvirt-daemon-system

# Crear VM
virt-install --name test-x11 --ram 2048 --disk size=20 --cdrom ubuntu.iso
```

### Contenedor Docker (Limitado)

**Nota:** Docker con X11 requiere configuración especial y no simula completamente un entorno de escritorio real.

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    libx11-dev \
    xorg \
    && rm -rf /var/lib/apt/lists/*

COPY . /app
WORKDIR /app

RUN make

# Requiere X11 forwarding del host
CMD ["./x11_keylogger"]
```

---

## Verificación Post-Instalación

### Checklist

- [ ] Compilación exitosa sin errores
- [ ] Archivo `x11_keylogger` es ejecutable
- [ ] Sistema usa X11 (no Wayland)
- [ ] Variable `$DISPLAY` está definida
- [ ] Servidor X11 está corriendo
- [ ] Has leído las advertencias legales
- [ ] Comprendes las implicaciones éticas
- [ ] Solo usarás en sistemas propios o autorizados

### Prueba Básica

```bash
# Test 1: Verificar que el ejecutable existe
test -x ./x11_keylogger && echo "✓ Ejecutable OK" || echo "✗ Error"

# Test 2: Verificar conexión X11
xdpyinfo > /dev/null 2>&1 && echo "✓ X11 OK" || echo "✗ X11 no disponible"

# Test 3: Verificar permisos de escritura
touch keylog.txt && rm keylog.txt && echo "✓ Permisos OK" || echo "✗ Sin permisos"
```

---

## Soporte y Recursos

### Documentación

- `README.md` - Vista general y uso básico
- `DOCUMENTACION.md` - Documentación técnica completa
- `SEGURIDAD.md` - Guía de seguridad y protección
- `INSTALACION.md` - Este archivo

### Reporte de Problemas

Si encuentras problemas:

1. Verifica que has seguido todos los pasos
2. Revisa la sección de solución de problemas
3. Consulta la documentación técnica
4. Abre un issue en GitHub con:
   - Distribución Linux y versión
   - Salida de `echo $XDG_SESSION_TYPE`
   - Mensajes de error completos
   - Pasos para reproducir

### Comunidad

- GitHub Issues: Reportar bugs o pedir ayuda
- GitHub Discussions: Preguntas generales y discusiones

---

## Consideraciones Legales

### Antes de Instalar

**Asegúrate de:**
- ✅ Comprender las leyes locales sobre software de monitoreo
- ✅ Tener autorización para usar en el sistema destino
- ✅ Usar solo con propósitos educativos legítimos
- ✅ No violar la privacidad de terceros
- ✅ Cumplir con regulaciones de privacidad (GDPR, etc.)

### Jurisdicciones

El uso no autorizado de keyloggers es ilegal en:
- Estados Unidos (Federal Wiretap Act)
- Unión Europea (GDPR)
- Reino Unido (Computer Misuse Act)
- Canadá (Criminal Code)
- Australia (Cybercrime Act)
- Y la mayoría de países del mundo

**Pena:** Puede incluir prisión, multas sustanciales y antecedentes penales.

---

## Conclusión

Siguiendo esta guía, deberías tener el keylogger educativo instalado correctamente en un entorno X11 para propósitos educativos legítimos.

**Recuerda:**
- 🎓 Uso educativo únicamente
- ⚖️ Cumplir con todas las leyes
- 🔒 Solo en sistemas propios o autorizados
- 🛡️ Promover la seguridad, no vulnerarla

---

**Última actualización:** 2025-11-02  
**Versión:** 1.0
