#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
build="${RAYRENDER_BUILD:-$root/build}"
impl="${1:-${RAYRENDER_IMPL:-cc}}"

if [[ $# -gt 0 ]]; then
    shift
fi

case "$impl" in
    cc)
        exec "$build/rayrender" "$@"
        ;;
    stock)
        exec "$build/rayrender-stock" "$@"
        ;;
    *)
        echo "usage: $0 [cc|stock]" >&2
        echo "  RAYRENDER_IMPL=cc|stock and RAYRENDER_BUILD=... also work." >&2
        exit 2
        ;;
esac
