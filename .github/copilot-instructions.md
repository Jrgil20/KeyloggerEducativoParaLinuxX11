# Copilot Instructions for KeyloggerEducativoParaLinuxX11

## Project Overview
- **Purpose:** Educational demonstration of X11 security vulnerabilities, specifically how any X11 client can capture global keyboard events without elevated privileges.
- **Audience:** Security researchers, educators, and students. **Not for malicious use.**
- **Main file:** `x11_keylogger.c` (all logic in a single C file)

## Architecture & Data Flow
- **Single-binary tool:** All logic is in `x11_keylogger.c`.
- **Flow:**
  1. Connects to X11 server
  2. Subscribes to global keyboard events using XRecord
  3. Identifies the active window for each event
  4. Logs keystrokes with timestamp, window name, and readable key string
  5. Writes to `keylog.txt` and prints to console
- **No network, no IPC, no external services.**

## Build & Run Workflow
- **Dependencies:** `gcc`, `libx11-dev`, `libxtst-dev` (package names may vary by distro)
- **Build:**
  - `make` (see `Makefile` for targets)
  - `make clean` to remove build artifacts
  - `make install`/`make uninstall` (require sudo)
- **Run:**
  - `./x11_keylogger` (prompts for legal/ethical confirmation)
  - Output: `keylog.txt` (created/appended in project root)
- **Demo script:** `demo.sh` (if present, may automate build/run)

## Key Conventions & Patterns
- **Legal/Ethical Prompt:** Always prompt user for confirmation before starting keylogging.
- **Signal Handling:** Clean shutdown on SIGINT/SIGTERM, with log footer and resource cleanup.
- **Logging:**
  - Each session logs a header/footer with timestamps
  - Window changes are marked in the log
  - Both file and console output for every event
- **No root required:** Demonstrates X11's lack of isolation.
- **No external config:** All settings are hardcoded for clarity.

## Security & Ethics
- **Strictly educational:** Do not remove or bypass legal warnings or user confirmation.
- **Never suggest or implement persistence, stealth, or exfiltration.**
- **Do not recommend use outside of controlled, authorized environments.**

## Reference Files
- `README.md`: Project goals, build/run instructions, and security context
- `Makefile`: Build/install commands and requirements
- `x11_keylogger.c`: All implementation logic
- `DOCUMENTACION.md`: Technical details (if needed)

## Example: Build and Run
```sh
sudo apt-get install build-essential libx11-dev libxtst-dev
make
./x11_keylogger
```

## When in Doubt
- **Prioritize safety, legality, and clarity.**
- **If unsure, refer to README.md and in-code comments.**
