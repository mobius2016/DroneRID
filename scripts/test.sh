#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p .build fixtures
cc -std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
 tests/test_rid.c flipper/core/rid.c flipper/vendor/opendroneid.c -lm -o .build/test_rid
.build/test_rid fixtures/synthetic-basic-id.rid
