#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
export PLATFORMIO_CORE_DIR="$PWD/.tools/platformio"
# No managed components are used; avoid an unnecessary dependency resolver.
export IDF_COMPONENT_MANAGER=0
exec venv/bin/pio run -d companion "$@"
