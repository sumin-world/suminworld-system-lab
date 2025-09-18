#!/usr/bin/env bash
# Signal Demo - Automated Test Script

set -euo pipefail

echo "========================================"
echo "  Signal Demo - Automated Test"
echo "========================================"
echo ""

# Compile
echo "[1] Compiling..."
gcc -O2 -Wall -Wextra -o signal_demo signal_demo.c

if [ $? -eq 0 ]; then
    echo "✓ Compilation successful"
else
    echo "✗ Compilation failed"
    exit 1
fi
echo ""

# Run automated test
echo "[2] Running automated test..."

# Start program with coprocess for proper PID tracking
coproc DEMO ( ./signal_demo )
DEMO_PID=$DEMO_PID

# Send menu choice "7" for automated test
printf "7\n" >&"${DEMO[1]}"

echo "✓ Program PID: ${DEMO_PID}"
echo ""

# Wait for test completion
echo "[3] Test in progress..."
wait "$DEMO_PID" 2>/dev/null || true

echo ""
echo "========================================"
echo "  Test completed!"
echo "========================================"
echo ""

# Manual testing guide
echo "Manual testing:"
echo ""
echo "1. Run program:"
echo "   $ ./signal_demo"
echo ""
echo "2. Send signals (another terminal):"
echo "   $ ps aux | grep signal_demo"
echo "   $ kill -USR1 [PID]"
echo "   $ kill -USR2 [PID]"
echo ""
echo "3. Job control:"
echo "   $ ./signal_demo &"
echo "   $ jobs"
echo "   $ fg"
echo ""

echo "✓ Done"