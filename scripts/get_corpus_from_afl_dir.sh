#!/usr/bin/env bash

#This gets the corpus from an afl output directory, and puts it in a single directory

#Todo make this parrallel

#This is the afl output
INPUT_DIR="$1"

if [ -z "$INPUT_DIR" ]; then
    echo "Usage: $0 <input_dir>"
    exit 1
fi

OUTPUT_DIR="$2"

if [ -z "$OUTPUT_DIR" ]; then
    echo "Usage: $0 <input_dir> <output_dir>"
    exit 1
fi

if ! [ -d "$OUTPUT_DIR" ]; then
    mkdir -p "$OUTPUT_DIR"
fi

wanted_dirs=("queue" "crashes" "hangs")

for dir in "$INPUT_DIR"/*/*; do
  [[ -d "$dir" ]] || continue   # only directories

  for wanted in "${wanted_dirs[@]}"; do

    if [[ "$(basename "$dir")" == "$wanted" ]]; then
      echo "Processing $dir"

      for file in "$dir"/*; do
        [[ -f "$file" ]] || continue
        echo "File: $file"
        cp "$file" "$OUTPUT_DIR/$(basename "$file")"
      done
    fi
done

