#!/bin/bash

START_TIME=$(date +%s)
CMD_RUN="$1"
mkdir -p logs
LOG_FILE="logs/fileops_$(date +%Y%m%d_%H%M%S).log"
log_execution() {
    local exit_code=$1
    local end_time=$(date +%s)
    local diff=$((end_time - START_TIME))
    echo "Subcomanda: $CMD_RUN" > "$LOG_FILE"
    echo "Timestamp start: $START_TIME" >> "$LOG_FILE"
    echo "Timestamp end: $end_time" >> "$LOG_FILE"
    echo "Durata (secunde): $diff" >> "$LOG_FILE"
    echo "Exit code: $exit_code" >> "$LOG_FILE"
    exit "$exit_code"
}
trap 'log_execution $?' EXIT
case "$CMD_RUN" in
   init)
        echo "Start init"
        mkdir -p bin src include data logs reports tmp/obj tests doc tools
        if ! command -v gcc &> /dev/null; then
            echo "Compilatorul gcc nu este instalat"
            exit 1
        fi
        echo "Init finalizat cu succes"
        ;;
build)
        echo "Start build"
        SRC_DIR="src"
        if [[ "$2" == "--src" && -n "$3" ]]; then
            SRC_DIR="$3"
        fi
        MODULES=()
        MAINS=()
        recursiv() {
            local dir="$1"
            local item
            for item in "$dir"/*; do
                if [[ -d "$item" ]]; then
                    recursiv "$item"
                elif [[ -f "$item" && "$item" == *.c ]]; then
                    local nume_fisier=$(basename "$item")
                    if [[ "$nume_fisier" == main_*.c ]]; then
                        MAINS+=("$item")
                    else
                        MODULES+=("$item")
                    fi
                fi
            done
        }
        if [[ -d "$SRC_DIR" ]]; then
            recursiv "$SRC_DIR"
        fi
        for src_file in "${MODULES[@]}"; do
            filename=$(basename "$src_file")
            obj_file="tmp/obj/${filename%.c}.o"
            if [[ ! -f "$obj_file" || "$src_file" -nt "$obj_file" ]]; then
                gcc $CFLAGS -c "$src_file" -o "$obj_file" -Iinclude -lcrypto
            fi
        done
        for main_file in "${MAINS[@]}"; do
            filename=$(basename "$main_file")
            exe_name="${filename#main_}"
            exe_name="${exe_name%.c}"
            shopt -s nullglob
            obj_files=(tmp/obj/*.o)
            shopt -u nullglob
            gcc $CFLAGS "$main_file" "${obj_files[@]}" -o "bin/$exe_name" -Iinclude -lcrypto
        done
        echo "Build finalizat cu succes"
        ;;
   clean)
        echo "Start clean"
        rm -rf tmp/obj/* bin/*
        echo "Clean finalizat cu succes"
        ;;
   run)
       echo "Start run"
       if [[ "$2" != "--" ]]; then
            echo "Lipsa separator --"
            exit 1
        fi
        EXE_NAME="$3"
        EXE_PATH="bin/$EXE_NAME"
        shift 3
        if [[ -x "$EXE_PATH" ]]; then
            echo "Rulez: $EXE_NAME..."
            "$EXE_PATH" "$@"
        else
            echo "Executabilul $EXE_PATH nu exista sau nu are permisiuni de rulare."
            exit 1
        fi
        ;;
   test)
        echo "Start test"
        REPORT_FILE="reports/T2_tests.txt"
        > "$REPORT_FILE"
        FAILED=0
        shopt -s globstar
        for test_script in tests/**/*.sh; do
            if [[ -f "$test_script" && -x "$test_script" ]]; then
                if "$test_script"; then
                    echo "$(basename "$test_script"): PASS" >> "$REPORT_FILE"
                else
                    echo "$(basename "$test_script"): FAIL" >> "$REPORT_FILE"
                    FAILED=1
                fi
            fi
        done
        shopt -u globstar
        if [ $FAILED -eq 1 ]; then
            exit 1
        fi
        echo "Test finalizat cu succes"
        ;;
    *)
        echo "Comanda invalida"
        exit 1
        ;;
esac
