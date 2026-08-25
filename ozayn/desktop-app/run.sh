#!/bin/bash
# Launch Ozayn Desktop (PyQt6)
DIR="$(cd "$(dirname "$0")" && pwd)"

if [ ! -d "$DIR/venv" ]; then
    echo "Setting up Python environment..."
    python3 -m venv "$DIR/venv"
    source "$DIR/venv/bin/activate"
    pip install -q PyQt6 requests
else
    source "$DIR/venv/bin/activate"
fi

echo "Starting Ozayn Desktop..."
python3 "$DIR/main.py"
