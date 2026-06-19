#!/bin/bash

echo "Start test T4"

TEST_ROOT="tmp/test_t4_tree"
TEST_DB="data/test_inventory.db"

echo "1. Gnerare mediu de test în $TEST_ROOT..."
rm -rf "$TEST_ROOT" "$TEST_DB"
mkdir -p "$TEST_ROOT/subdir1"
mkdir -p "$TEST_ROOT/subdir2"

echo "Fisier 1" > "$TEST_ROOT/f1.txt"
echo "Fisier 2" > "$TEST_ROOT/subdir1/f2.log"
echo "Fisier 3" > "$TEST_ROOT/subdir2/f3.dat"
ln -s "$TEST_ROOT/f1.txt" "$TEST_ROOT/symlink.txt" 2>/dev/null || true
 
echo "2. Rulăm managerul cu 2 workeri..."
./bin/fileops_manager --root "$TEST_ROOT" --workers 2 --db "$TEST_DB" 
echo "3. Verificăm existența bazei de date..."
if [ ! -f "$TEST_DB" ]; then
    echo "FAIL: Baza de date $TEST_DB nu a fost creată!"
    exit 1
fi
echo "OK: Baza de date există."
echo "4. Rulăm verificarea integrității (--verify)..."
if ! ./bin/fileops_manager --db "$TEST_DB" --verify; then
    echo "FAIL: --verify a raportat baza de date ca fiind invalidă!"
    exit 1
fi
echo "OK: Verificare trecută." 
echo "5. Verificăm conținutul extras prin --dump..."
DUMP_OUTPUT=$(./bin/fileops_manager --db "$TEST_DB" --dump)
if ! echo "$DUMP_OUTPUT" | grep -q "magic INV4"; then
    echo "FAIL: Lipsă 'magic INV4' în dump."
    exit 1
fi
if ! echo "$DUMP_OUTPUT" | grep -q "complete 1"; then
    echo "FAIL: Lipsă 'complete 1' în dump."
    exit 1
fi
if ! echo "$DUMP_OUTPUT" | grep -q "worker_count 2"; then
    echo "FAIL: 'worker_count' nu este 2."
    exit 1
fi
if ! echo "$DUMP_OUTPUT" | grep -q "file_record_count 3"; then
    echo "FAIL: 'file_record_count' nu este 3."
    echo "DUMP RETURNAT:"
    echo "$DUMP_OUTPUT"
    exit 1
fi
echo "OK: Toate câmpurile din --dump sunt corecte."
echo "==> Test finalizat cu succes! (PASS)"
rm -rf "$TEST_ROOT" "$TEST_DB"
exit 0
