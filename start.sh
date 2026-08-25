#!/bin/bash
# Ozayn Local Development Startup Script
# Starts all services for local testing

set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
ML_PID=""
WS_PID=""
PHP_PID=""

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

cleanup() {
    echo ""
    echo -e "${YELLOW}Shutting down Ozayn...${NC}"
    [ -n "$ML_PID" ] && kill $ML_PID 2>/dev/null
    [ -n "$WS_PID" ] && kill $WS_PID 2>/dev/null
    [ -n "$PHP_PID" ] && kill $PHP_PID 2>/dev/null
    echo -e "${GREEN}All services stopped.${NC}"
    exit 0
}

trap cleanup SIGINT SIGTERM

echo -e "${CYAN}"
echo "  ╔═══════════════════════════════════════╗"
echo "  ║         OZAYN LOCAL DEV SERVER        ║"
echo "  ║     Personal AI Digital Twin          ║"
echo "  ╚═══════════════════════════════════════╝"
echo -e "${NC}"

# Check dependencies
echo -e "${YELLOW}[1/5] Checking dependencies...${NC}"

missing=()
for cmd in php python3; do
    command -v $cmd &>/dev/null || missing+=($cmd)
done

if [ ${#missing[@]} -gt 0 ]; then
    echo -e "${RED}Missing: ${missing[*]}${NC}"
    echo "Install them first:"
    echo "  sudo apt install php php-sqlite3 python3 python3-pip python3-venv"
    exit 1
fi
echo -e "${GREEN}  PHP: $(php -v | head -1)${NC}"
echo -e "${GREEN}  Python: $(python3 --version)${NC}"

# Setup Python environment
echo -e "${YELLOW}[2/5] Setting up Python ML environment...${NC}"

cd "$DIR/ozayn/ml"
if [ ! -d "venv" ]; then
    echo "  Creating virtual environment..."
    python3 -m venv venv
fi

source venv/bin/activate
pip install -q --upgrade pip 2>/dev/null
pip install -q -r requirements.txt 2>/dev/null
echo -e "${GREEN}  Python environment ready${NC}"

# Initialize database
echo -e "${YELLOW}[3/5] Initializing database...${NC}"

cd "$DIR"
if [ ! -f "ozayn/database/ozayn.db" ]; then
    php ozayn/install.php
    echo -e "${GREEN}  Database created${NC}"
else
    echo -e "${GREEN}  Database exists${NC}"
fi

# Start ML Server
echo -e "${YELLOW}[4/5] Starting ML WebSocket server (port 8765)...${NC}"

cd "$DIR/ozayn/ml"
ML_HOST=127.0.0.1 ML_PORT=8765 python3 server.py &
ML_PID=$!
sleep 1

if kill -0 $ML_PID 2>/dev/null; then
    echo -e "${GREEN}  ML server running (PID: $ML_PID)${NC}"
else
    echo -e "${RED}  ML server failed to start${NC}"
fi

# Start PHP server
echo -e "${YELLOW}[5/5] Starting PHP development server (port 8000)...${NC}"

cd "$DIR"
php -S 127.0.0.1:8000 router.php > /dev/null 2>&1 &
PHP_PID=$!
sleep 1

if kill -0 $PHP_PID 2>/dev/null; then
    echo -e "${GREEN}  PHP server running (PID: $PHP_PID)${NC}"
else
    echo -e "${RED}  PHP server failed to start${NC}"
fi

# Print status
echo ""
echo -e "${CYAN}═══════════════════════════════════════════${NC}"
echo -e "${GREEN}  Ozayn is running!${NC}"
echo -e "${CYAN}═══════════════════════════════════════════${NC}"
echo ""
echo -e "  Web UI:       ${GREEN}http://localhost:8000/ozayn${NC}"
echo -e "  ML Server:    ${GREEN}ws://localhost:8765${NC}"
echo -e "  Database:     ${GREEN}ozayn/database/ozayn.db${NC}"
echo ""
echo -e "  ${YELLOW}Press Ctrl+C to stop all services${NC}"
echo -e "  ${YELLOW}Press 'd' to open Desktop App${NC}"
echo ""

# Listen for 'd' key to launch desktop app
read_char() {
    stty raw -echo
    char=$(dd bs=1 count=1 2>/dev/null)
    stty -raw echo
    echo "$char"
}

while true; do
    if read_char | grep -qi "d"; then
        if [ -d "$DIR/ozayn/desktop/node_modules" ]; then
            echo -e "${CYAN}Launching Desktop App...${NC}"
            cd "$DIR/ozayn/desktop" && npx electron . &
        else
            echo -e "${RED}Desktop not installed. Run: cd ozayn/desktop && npm install${NC}"
        fi
    fi
done &

# Keep running
wait
