#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
export UFBT_HOME="$PWD/.tools/ufbt"
root="$PWD"
cd flipper
"$root/venv/bin/ufbt" "$@"
