#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
GAME="$HERE/.."
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
ZGDK="$ZGDK_PATH"
cc -std=gnu11 -w -g -fsanitize=address,undefined \
    '-D__sfr=static volatile unsigned char' '-D__at(x)=' \
    -I"$HERE/stub" -I"$GAME/src" -I"$ZGDK/include" \
    -I"$ZOS_PATH/sdk/sdcc/include" -I"$ZVB_SDK_PATH/include" \
    "$HERE/host_stream.c" "$GAME/src/stream.c" "$GAME/src/stream_shift.c" \
    "$ZGDK/src/tilemap/flags/"*.c "$ZGDK/src/camera/"*.c \
    "$ZGDK/src/tilemap/stream"*.c -o "$OUT/stream"
"$OUT/stream"
