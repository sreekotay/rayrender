#!/bin/sh
exec "$(cd "$(dirname "$0")/.." && pwd)/rlsw-cc/tools/gen.sh" "$@"
