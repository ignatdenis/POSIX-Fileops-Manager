#!/bin/bash

TEST_DIR="/dev/shm/test_t5_stress"
DB_FILE="data/t5_inventory.db"
IPC_FILE="data/t5_ipc.mmap"
PID_FILE="data/manager.pid"
LOG_FILE="reports/t5_manager.log"

mkdir -p reports
rm -rf "$TEST_DIR" "$DB_FILE" "$IPC_FILE" "$PID_FILE" "$LOG_FILE"
mkdir -p "$TEST_DIR"

echo "Generare 50.000 de fisiere in RAM..."
for i in {1..100}; do
    mkdir -p "$TEST_DIR/dir_$i"
    for j in {1..10}; do
        mkdir -p "$TEST_DIR/dir_$i/sub_$j"
        for k in {1..50}; do
            echo "Date" > "$TEST_DIR/dir_$i/sub_$j/f_$k.txt"
        done
    done
done

echo "Lansare Manager..."
./tools/fileops.sh run -- fileops_manager \
    --root "$TEST_DIR" \
    --workers 4 \
    --ipc "$IPC_FILE" \
    --db "$DB_FILE" \
    --simulate-work-ms 2 \
    --graceful-timeout 4 \
    --pid-file "$PID_FILE" > "$LOG_FILE" 2>&1 &

BASH_BG_PID=$!

retries=0
while [ ! -f "$PID_FILE" ]; do
    sleep 0.1
    let retries+=1
    if [ $retries -gt 50 ]; then
        echo "FAIL: Fisierul PID nu a fost generat."
        kill $BASH_BG_PID 2>/dev/null
        exit 1
    fi
done

MANAGER_PID=$(cat "$PID_FILE")
echo "Manager PID: $MANAGER_PID"

sleep 2
echo "Trimitere SIGUSR1..."
kill -USR1 $MANAGER_PID

sleep 2
echo "Trimitere SIGTERM..."
kill -TERM $MANAGER_PID

echo "Asteptare oprire manager..."
wait $BASH_BG_PID

echo "--- Rezultate ---"

if grep -q "STATUS queued_jobs" "$LOG_FILE"; then
    echo "PASS: Status gasit in log."
else
    echo "FAIL: Status lipsa."
    exit 1
fi

if grep -q "T5MSG type=WORKER_EXITING" "$LOG_FILE"; then
    echo "PASS: Mesaje pipe gasite."
else
    echo "FAIL: Mesaje pipe lipsa."
    exit 1
fi

if ./tools/fileops.sh run -- fileops_manager --db "$DB_FILE" --verify > /dev/null; then
    echo "PASS: Baza de date valida (--verify)."
else
    echo "FAIL: Baza de date invalida."
    exit 1
fi

DUMP_OUTPUT=$(./tools/fileops.sh run -- fileops_manager --db "$DB_FILE" --dump)
if echo "$DUMP_OUTPUT" | grep -q "complete 0"; then
    echo "PASS: complete=0 setat corect."
else
    echo "FAIL: complete=0 lipsa in dump."
    exit 1
fi

ZOMBIES=$(ps a | grep fileops_worker | grep -v grep || true)
if [ -n "$ZOMBIES" ]; then
    echo "FAIL: Procese zombie ramase in fundal."
    exit 1
else
    echo "PASS: Fara procese zombie."
fi

echo "SUCCESS: Test trecut cu succes."

rm -rf "$TEST_DIR" "$DB_FILE" "$IPC_FILE" "$PID_FILE"
exit 0
