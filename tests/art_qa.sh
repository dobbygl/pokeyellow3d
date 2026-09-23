#!/usr/bin/env bash
# Private, reproducible A1 catalog/reference gate; no cartridge data enters git.
set -euo pipefail
if (( $# != 4 )); then
    echo "Usage: $0 ROM PALLET_9_7_WITH_PARTY FIVE_RESIDENT_MAPS_STATE V041_BENCHMARK_BINARY" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
cmake --build "$project/build" --target all pallet_render_smoke art_benchmark --parallel 4
ctest --test-dir "$project/build" --output-on-failure
python3 "$project/tests/art_qa.py" "$project/build/pallet_render_smoke" "$project/build/art_benchmark" \
    "$(realpath -- "$1")" "$(realpath -- "$2")" "$(realpath -- "$3")" \
    "$(realpath -- "$4")" "$project/build/qa"
