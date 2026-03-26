#!/usr/bin/env sh
set -e

SDK_ROOT="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
if [ -z "$SDK_ROOT" ]; then
  echo "ANDROID_HOME or ANDROID_SDK_ROOT is not set."
  exit 1
fi

D8_BIN=$(ls "$SDK_ROOT"/build-tools/*/d8 2>/dev/null | sort | tail -n 1)
if [ -z "$D8_BIN" ] || [ ! -x "$D8_BIN" ]; then
  echo "d8 not found under $SDK_ROOT/build-tools." >&2
  exit 1
fi

OUT_DIR="$(pwd)/out"
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

javac -d "$OUT_DIR" Hello.java
jar cf "$OUT_DIR/hello.jar" -C "$OUT_DIR" .

"$D8_BIN" --min-api 21 --output "$OUT_DIR" "$OUT_DIR/hello.jar"

cp "$OUT_DIR/classes.dex" ../../app/src/main/assets/hello.dex

echo "Wrote app/src/main/assets/hello.dex"
