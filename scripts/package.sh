#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p dist
cp flipper/dist/drone_rid.fap dist/
cp fixtures/synthetic-basic-id.rid dist/
venv/bin/python .tools/platformio/packages/tool-esptoolpy/esptool.py --chip esp32s2 merge_bin \
 -o dist/companion-merged.bin --flash_mode dio --flash_size 4MB --flash_freq 80m \
 0x1000 companion/.pio/build/devboard/bootloader.bin \
 0x8000 companion/.pio/build/devboard/partitions.bin \
 0x10000 companion/.pio/build/devboard/firmware.bin
