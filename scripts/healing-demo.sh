#!/usr/bin/env bash
# healing-demo.sh — Scripted self-healing scenario with live dashboard
# Run: ./scripts/healing-demo.sh
# Then open http://localhost:8080 to watch

set -e
DIR="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$DIR/build/app/matterthreads"

if [ ! -x "$BIN" ]; then
    echo "Build first: cmake -B build && cmake --build build"
    exit 1
fi

# Feed commands into the interactive REPL via a named pipe
FIFO=$(mktemp -u /tmp/mt-demo.XXXX)
mkfifo "$FIFO"

cleanup() {
    echo "quit" > "$FIFO" 2>/dev/null || true
    rm -f "$FIFO"
    pkill -f mt_broker 2>/dev/null || true
    pkill -f mt_node 2>/dev/null || true
    echo ""
    echo "Demo stopped."
}
trap cleanup EXIT INT TERM

# Start the simulation reading commands from the FIFO
# Use tail -f so the pipe stays open across writes
tail -f "$FIFO" | "$BIN" --topology van --dashboard 8080 &
SIM_PID=$!
sleep 4

echo "============================================"
echo " MatterThreads Self-Healing Demo"
echo " Dashboard: http://localhost:8080"
echo "============================================"
echo ""

send() {
    echo "$1" > "$FIFO"
    sleep 0.3
}

pause() {
    echo ""
    echo "--- [$1] $2 ---"
    echo ""
    sleep "$3"
}

# === Act 1: Healthy mesh ===
pause "00:00" "Mesh is healthy — all 4 nodes running" 2
send "status"
sleep 2
send "discover"
sleep 2
send "timeline"
sleep 1

pause "00:08" "Phone discovers services via DNS-SD" 3

# === Act 2: Cargo sensor crash ===
pause "00:12" "FAULT: Cargo sensor (Node 2) crashes!" 1
send "crash 2"
sleep 3
send "status"
sleep 2
send "healing"
sleep 2
send "timeline"

pause "00:22" "Mesh detects Node 2 lost — self-healing kicks in" 4

# === Act 3: Sensor comes back ===
pause "00:28" "RECOVERY: Restarting cargo sensor (Node 2)" 1
send "restart 2"
sleep 3
send "status"
sleep 2
send "healing"
sleep 2
send "timeline"

pause "00:38" "Node 2 reattached — subscriptions recovered" 4

# === Act 4: Engine crank power dip ===
pause "00:44" "FAULT: Engine cranking — voltage dip!" 1
send "crank"
sleep 4
send "status"
sleep 2
send "timeline"

pause "00:54" "Relay router (Node 1) rebooted after crank" 4

# === Act 5: Backhaul failure ===
pause "01:00" "FAULT: Cellular backhaul goes down (entering tunnel)" 1
send "backhaul down"
sleep 3
send "status"
sleep 2
send "timeline"

pause "01:10" "Phone isolated — mesh continues locally" 4

# === Act 6: Backhaul recovery ===
pause "01:16" "RECOVERY: Exiting tunnel — backhaul restored" 1
send "backhaul up"
sleep 3
send "status"
sleep 2
send "discover"
sleep 2
send "timeline"

pause "01:26" "Phone reconnected — services re-discovered" 4

# === Act 7: Multiple faults ===
pause "01:32" "CHAOS: Relay crash + backhaul down simultaneously!" 1
send "crash 1"
sleep 1
send "backhaul down"
sleep 4
send "status"
sleep 2
send "timeline"

pause "01:44" "Worst case: relay down + no backhaul" 4

# === Act 8: Full recovery ===
pause "01:50" "RECOVERY: Restoring everything" 1
send "restart 1"
sleep 3
send "backhaul up"
sleep 3
send "status"
sleep 2
send "healing"
sleep 2
send "timeline"

pause "02:04" "Full mesh restored — all nodes healthy" 3

# === Finale ===
echo ""
echo "============================================"
echo " Demo complete! Final state:"
echo "============================================"
send "status"
sleep 2
send "metrics"
sleep 2
send "timeline"
sleep 3

echo ""
echo "Press Ctrl+C to stop, or the demo will exit in 30s..."
sleep 30
