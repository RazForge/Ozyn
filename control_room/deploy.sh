#!/bin/bash
# Ozayn Control Room — Deploy Script
# Installs dependencies, builds, and sets up autostart.

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log()  { echo -e "${GREEN}[+]${NC} $1"; }
warn() { echo -e "${YELLOW}[!]${NC} $1"; }
err()  { echo -e "${RED}[x]${NC} $1"; exit 1; }

# ── 1. Check root for system deps ──
if [ "$EUID" -ne 0 ]; then
    err "Run with sudo: sudo ./deploy.sh"
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# ── 2. Install system dependencies ──
log "Installing system dependencies..."
apt-get update -qq 2>/dev/null || true
apt-get install -y -qq \
    build-essential \
    gcc make \
    libx11-dev \
    libxtst-dev \
    libasound2-dev \
    pkg-config

# ── 3. Install OpenCV (for gesture tracking) ──
if ! pkg-config --exists opencv4 2>/dev/null; then
    log "Installing OpenCV..."
    apt-get install -y -qq libopencv-dev
else
    log "OpenCV already installed"
fi

# ── 4. Install Vosk (for voice recognition) ──
if ! ldconfig -p | grep -q libvosk; then
    log "Installing Vosk SDK..."
    VOSK_VERSION="0.3.45"
    cd /tmp
    wget -q "https://github.com/alphacep/vosk-api/releases/download/v${VOSK_VERSION}/vosk-linux-x86_64-${VOSK_VERSION}.zip" -O vosk.zip || warn "Vosk download failed — voice will use stub"
    if [ -f vosk.zip ]; then
        unzip -qo vosk.zip -d /opt/vosk
        cp /opt/vosk/libvosk.so /usr/lib/
        cp /opt/vosk/vosk_api.h /usr/include/
        ldconfig
        rm vosk.zip
    fi
else
    log "Vosk already installed"
fi

# ── 5. Download Vosk language models ──
MODELS_DIR="/opt/vosk/models"
mkdir -p "$MODELS_DIR"

if [ ! -d "$MODELS_DIR/en-us" ]; then
    log "Downloading English model (vosk-model-small-en-us-0.15)..."
    cd /tmp
    wget -q "https://alphacephei.com/vosk/models/vosk-model-small-en-us-0.15.zip" -O en.zip || warn "English model download failed"
    if [ -f en.zip ]; then
        unzip -qo en.zip -d "$MODELS_DIR"
        mv "$MODELS_DIR/vosk-model-small-en-us-0.15" "$MODELS_DIR/en-us"
        rm en.zip
    fi
fi

if [ ! -d "$MODELS_DIR/am-et" ]; then
    log "Downloading Amharic model..."
    cd /tmp
    wget -q "https://alphacephei.com/vosk/models/vosk-model-small-am-0.4.zip" -O am.zip || warn "Amharic model download failed"
    if [ -f am.zip ]; then
        unzip -qo am.zip -d "$MODELS_DIR"
        mv "$MODELS_DIR/vosk-model-small-am-0.4" "$MODELS_DIR/am-et"
        rm am.zip
    fi
fi

# ── 6. Build control room ──
log "Building control room..."
cd "$SCRIPT_DIR"
make clean
make all

if [ ! -f build/ozayn_control ]; then
    err "Build failed"
fi

log "Build successful: build/ozayn_control"

# ── 7. Create systemd service (optional) ──
SERVICE_FILE="/etc/systemd/system/ozayn-control.service"
cat > "$SERVICE_FILE" << 'EOF'
[Unit]
Description=Ozayn Control Room
After=graphical.target

[Service]
Type=simple
ExecStart=/usr/bin/env bash -c 'cd CONTROL_DIR && ./build/ozayn_control'
Restart=on-failure
RestartSec=5
User=USER_NAME
Environment=DISPLAY=:0
Environment=HOME=/home/USER_NAME

[Install]
WantedBy=multi-user.target
EOF

sed -i "s|CONTROL_DIR|$SCRIPT_DIR|g" "$SERVICE_FILE"
sed -i "s|USER_NAME|$SUDO_USER|g" "$SERVICE_FILE"

log "Systemd service created: $SERVICE_FILE"

# ── 8. Create launcher script ──
cat > "$SCRIPT_DIR/start.sh" << 'LAUNCHER'
#!/bin/bash
cd "$(dirname "$0")"
echo "Starting Ozayn Control Room..."
./build/ozayn_control
LAUNCHER
chmod +x "$SCRIPT_DIR/start.sh"

log "Launcher created: ./start.sh"

# ── Done ──
echo ""
echo "═══════════════════════════════════════════"
echo "  Ozayn Control Room — Deploy Complete"
echo "═══════════════════════════════════════════"
echo ""
echo "  Run manually:  ./start.sh"
echo "  Run service:   sudo systemctl enable ozayn-control"
echo "                 sudo systemctl start ozayn-control"
echo "  Logs:          journalctl -u ozayn-control -f"
echo "  Stop:          sudo systemctl stop ozayn-control"
echo ""
echo "  Features:"
echo "    Voice:       English + Amharic (Vosk offline)"
echo "    Gesture:     OpenCV motion+skin tracking"
echo "    Keyboard:    X11 on-screen keyboard"
echo "    IPC:         Unix domain socket"
echo ""
