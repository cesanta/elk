#!/bin/bash

INPUT_DIR="$1"
OUTPUT_ROOT="./diff_outputs"
BINARIES=("cli-normal" "cli-san1" "cli-san2" "cli-san3")


# Check input directory
if [[ -z "$INPUT_DIR" ]]; then
    echo "Usage: $0 <input_directory>"
    exit 1
fi

rm -rf $OUTPUT_ROOT
mkdir -p "$OUTPUT_ROOT"

for file in "$INPUT_DIR"/*; do
    [[ -f "$file" && "$(basename "$file")" != "README.txt" ]] || continue

    # Temporary files to store outputs
    TMP_DIR=$(mktemp -d)
    declare -A OUTPUTS

    # Run each binary and save output
    for bin in "${BINARIES[@]}"; do
        if [[ -x "./$bin" ]]; then
            OUT_FILE="$TMP_DIR/$bin.out"
            "./$bin" "$file" > "$OUT_FILE" 2>&1
            OUTPUTS["$bin"]="$OUT_FILE"
        else
            echo "Binary $bin not found or not executable"
        fi
    done

    # Compare outputs
    first="${BINARIES[0]}"
    all_same=true
    for bin in "${BINARIES[@]:1}"; do
        if ! cmp -s "${OUTPUTS[$first]}" "${OUTPUTS[$bin]}"; then
            all_same=false
            break
        fi
    done

    # If outputs differ, create a folder and save outputs + input
    if [ "$all_same" = false ]; then
        FILE_BASE=$(basename "$file")
        DIFF_DIR="$OUTPUT_ROOT/$FILE_BASE"
        mkdir -p "$DIFF_DIR"

        # Copy all outputs
        for bin in "${BINARIES[@]}"; do
            cp "${OUTPUTS[$bin]}" "$DIFF_DIR/$bin.out"
        done

        # Save the original input as 'input'
        cp "$file" "$DIFF_DIR/input"

        echo "Outputs differ for $file — saved in $DIFF_DIR"
    fi

    # Clean up temp
    rm -rf "$TMP_DIR"
done
