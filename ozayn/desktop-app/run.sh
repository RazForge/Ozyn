#!/bin/bash
# Ozayn Desktop — Build C core and launch PyQt6 app
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CORE_DIR="$SCRIPT_DIR/core"
VENV_DIR="$SCRIPT_DIR/venv"

echo "=== Ozayn Desktop ==="

# Step 1: Build C core
echo "Building C core..."
cd "$CORE_DIR"
make clean && make all
echo "Core built: $CORE_DIR/build/libozayn_core.so"

# Step 2: Setup Python venv if needed
cd "$SCRIPT_DIR"
if [ ! -d "$VENV_DIR" ]; then
    echo "Creating Python virtual environment..."
    python3 -m venv "$VENV_DIR"
fi

source "$VENV_DIR/bin/activate"
pip install -q PyQt6 requests 2>/dev/null || true

# Step 3: Launch app
echo "Launching Ozayn Desktop..."
export LD_LIBRARY_PATH="$CORE_DIR/build:$LD_LIBRARY_PATH"
exec python3 "$SCRIPT_DIR/main.py"
