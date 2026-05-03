#!/usr/bin/env bash
# Slopped together with chatgpt

set -euo pipefail

# Configuration
N_ROUNDS=5
INPUT_DIR="new_corpus"
TARGET="./normal"
JOBS="$(nproc)"

OUTPUT_DIR="minified-corpus"


for round in $(seq 1 "$N_ROUNDS"); do
    echo "[*] Round $round"

    CMIN_DIR="cmin-round-$round"
    TMIN_DIR="tmin-round-$round"

    mkdir -p "$CMIN_DIR" "$TMIN_DIR"

    echo "  [+] minify-corpus (single instance)"
    afl-cmin \
        -i "$INPUT_DIR" \
        -o "$CMIN_DIR" \
        -t 500 \
        -m 100 \
        -- $TARGET

    export TMIN_DIR TARGET

    echo "  [+] minify-testcases (parallel)"
    find "$CMIN_DIR" -type f | parallel -j "$JOBS" '
        afl-tmin \
            -i {} \
            -o "$TMIN_DIR/{/}" \
            -t 500 \
            -m 100 \
            -- "$TARGET"
    '


    INPUT_DIR="$TMIN_DIR"
done

cp -r "$TMIN_DIR" "$OUTPUT_DIR"

# Delete intermediate directories
for round in $(seq 1 "$N_ROUNDS"); do
    CMIN_DIR="cmin-round-$round"
    TMIN_DIR="tmin-round-$round"
    rm -rf "$TMIN_DIR" "$CMIN_DIR"
done
