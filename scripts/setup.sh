#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
if [ ! -x venv/bin/python ]; then python3 -m venv venv; fi
venv/bin/python -m pip install -r requirements-dev.txt
UFBT_HOME="$PWD/.tools/ufbt" venv/bin/ufbt update --channel=release
