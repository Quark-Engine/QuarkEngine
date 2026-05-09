#!/usr/bin/env bash

set -euo pipefail

SOURCE="assets/lang/english.json"
LANG_DIR="assets/lang"
TARGETS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --source)
            SOURCE="$2"
            shift 2
            ;;
        --lang-dir)
            LANG_DIR="$2"
            shift 2
            ;;
        --targets)
            IFS=',' read -r -a TARGETS <<< "$2"
            shift 2
            ;;
        *)
            echo "Unknown argument: $1" >&2
            exit 1
            ;;
    esac
done

if [[ ! -f "$SOURCE" ]]; then
    echo "Source file not found: $SOURCE" >&2
    exit 1
fi

if [[ ! -d "$LANG_DIR" ]]; then
    echo "Language directory not found: $LANG_DIR" >&2
    exit 1
fi

if ! command -v jq >/dev/null 2>&1; then
    echo "jq is required but was not found in PATH" >&2
    exit 1
fi

source_abs="$(cd "$(dirname "$SOURCE")" && pwd)/$(basename "$SOURCE")"

collect_target_files() {
    if [[ ${#TARGETS[@]} -gt 0 ]]; then
        for target in "${TARGETS[@]}"; do
            if [[ "$target" == *.json ]]; then
                printf '%s\n' "$LANG_DIR/$target"
            else
                printf '%s\n' "$LANG_DIR/$target.json"
            fi
        done
        return
    fi

    find "$LANG_DIR" -maxdepth 1 -type f -name '*.json' | while IFS= read -r file; do
        file_abs="$(cd "$(dirname "$file")" && pwd)/$(basename "$file")"
        if [[ "$file_abs" != "$source_abs" ]]; then
            printf '%s\n' "$file"
        fi
    done
}

merge_filter='
def merge_missing($src; $dst):
  reduce ($src | keys_unsorted[]) as $key
    ({data: $dst, added: 0};
      if (.data | has($key)) then
        if (($src[$key] | type) == "object" and (.data[$key] | type) == "object") then
          (merge_missing($src[$key]; .data[$key])) as $nested
          | .data[$key] = $nested.data
          | .added += $nested.added
        else
          .
        end
      else
        .data[$key] = $src[$key]
        | .added += 1
      end
    );

merge_missing($source; $target)
'

while IFS= read -r file; do
    [[ -z "$file" ]] && continue

    if [[ ! -f "$file" ]]; then
        echo "Skipped missing target: $file" >&2
        continue
    fi

    tmp_file="$(mktemp)"
    result_json="$(jq -n \
        --slurpfile source "$SOURCE" \
        --slurpfile target "$file" \
        '$source[0] as $source | $target[0] as $target | '"$merge_filter"'')"

    added_keys="$(jq -r '.added' <<< "$result_json")"
    jq '.data' <<< "$result_json" > "$tmp_file"
    mv "$tmp_file" "$file"

    echo "$(basename "$file"): added $added_keys missing keys"
done < <(collect_target_files)
