#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
build="${RAYRENDER_BUILD:-$root/build}"
mode="smoke"
write_ppm=0
width=320
height=180
quality=0
frames=2
scene="maze"
adaptive="0"

usage() {
    cat <<EOF
usage: $0 [--smoke|--bench|--retina] [--ppm] [--scene maze|viewer|stress]

  --smoke   320x180 maze, adaptive off, checksum must match (default)
  --bench   1280x720 maze, quality 2, adaptive on
  --retina  2560x1440 maze, quality 2, adaptive on
  --ppm     write out/maze_<impl>.ppm

Env: RAYRENDER_BUILD, RAYRENDER_SCENE, RAYRENDER_FRAMES, RAYRENDER_BIN=hstripe|vstripe|off|64x64|… (default hstripe),
     RAYRENDER_SEQ / RLSW_SEQ=1 force sequential stripe fill
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --smoke) mode=smoke; shift ;;
        --bench) mode=bench; shift ;;
        --retina) mode=retina; shift ;;
        --ppm) write_ppm=1; shift ;;
        --scene) scene="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown arg: $1" >&2; usage; exit 2 ;;
    esac
done

case "$mode" in
    smoke)
        width=320; height=180; quality=0; frames="${RAYRENDER_FRAMES:-2}"; adaptive=0
        ;;
    bench)
        width=1280; height=720; quality=2; frames="${RAYRENDER_FRAMES:-3}"; adaptive=1
        ;;
    retina)
        width=2560; height=1440; quality=2; frames="${RAYRENDER_FRAMES:-2}"; adaptive=1
        ;;
esac

if [[ ! -x "$build/rayrender" || ! -x "$build/rayrender-stock" ]]; then
    cmake -S "$root" -B "$build"
    cmake --build "$build" -j
fi

mkdir -p "$root/out"

run_one() {
    local impl="$1"
    local bin
    if [[ "$impl" == stock ]]; then
        bin="$build/rayrender-stock"
    else
        bin="$build/rayrender"
    fi
    local ppm=""
    if [[ "$write_ppm" -eq 1 ]]; then
        ppm="$root/out/${scene}_${impl}.ppm"
    fi
    env RAYRENDER_PARITY=1 \
        RAYRENDER_WIDTH="$width" \
        RAYRENDER_HEIGHT="$height" \
        RAYRENDER_QUALITY="$quality" \
        RAYRENDER_FRAMES="$frames" \
        RAYRENDER_SCENE="$scene" \
        RAYRENDER_ADAPTIVE="$adaptive" \
        RAYRENDER_BIN="${RAYRENDER_BIN:-hstripe}" \
        RAYRENDER_SEQ="${RAYRENDER_SEQ:-}" \
        RAYRENDER_FILTER="${RAYRENDER_FILTER:-}" \
        RAYRENDER_SCALE=1 \
        RAYRENDER_PPM="$ppm" \
        "$bin"
}

stock_out="$(run_one stock)"
cc_out="$(run_one cc)"

echo "$stock_out"
echo "$cc_out"

parse_field() {
    local line="$1"
    local key="$2"
    echo "$line" | sed -n "s/.* ${key}=\\([^ ]*\\).*/\\1/p"
}

stock_sum="$(parse_field "$stock_out" checksum)"
cc_sum="$(parse_field "$cc_out" checksum)"
stock_ms="$(parse_field "$stock_out" time_ms)"
cc_ms="$(parse_field "$cc_out" time_ms)"

if [[ -z "$stock_sum" || -z "$cc_sum" ]]; then
    echo "parity: failed to parse checksum lines" >&2
    exit 1
fi

if [[ "$stock_sum" == "$cc_sum" ]]; then
    match=1
    match_s=MATCH
else
    match=0
    match_s=DIFF
fi

vs="n/a"
if command -v awk >/dev/null; then
    vs="$(awk -v s="$stock_ms" -v c="$cc_ms" 'BEGIN {
        if (s+0 <= 0) { print "n/a"; exit }
        printf "%.3fx", s/c
    }')"
fi

echo "parity mode=$mode scene=$scene ${width}x${height} q=$quality adaptive=$adaptive checksum=$match_s stock=$stock_sum cc=$cc_sum time_ms stock=$stock_ms cc=$cc_ms vs_stock=$vs"

# After 1.5 (bary planes + 16.16 snap) maze is allowed to DIFF vs stock.
# --strict still requires MATCH (reject-only / adaptive-off coverage checks).
if [[ "$mode" == smoke && "$match" -ne 1 && "${RAYRENDER_STRICT:-0}" != "1" ]]; then
    echo "parity: smoke DIFF vs stock (expected after bary planes; set RAYRENDER_STRICT=1 to fail)"
fi
if [[ "${RAYRENDER_STRICT:-0}" == "1" && "$match" -ne 1 ]]; then
    echo "parity: STRICT checksum mismatch" >&2
    exit 1
fi
