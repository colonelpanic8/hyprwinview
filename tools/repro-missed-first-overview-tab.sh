#!/usr/bin/env bash
set -euo pipefail

# Reproduces the current "first Super+Tab after directional focus is ignored" bug.
#
# This intentionally uses ydotool's virtual keyboard. On Ivan's keyd setup,
# ydotool KEY_LEFTALT is transformed into logical Super, matching the physical
# Super bindings seen by Hyprland.

iterations="${REPRO_ITERATIONS:-3}"
wait_after_focus="${REPRO_WAIT_AFTER_FOCUS:-1.0}"
settle_after_key="${REPRO_SETTLE_AFTER_KEY:-0.25}"
trace_file="${REPRO_TRACE_FILE:-/tmp/hypr-overview-bind.log}"
# The Hyprland Lua config only writes overview traces while this sentinel exists.
trace_enable_file="${REPRO_TRACE_ENABLE_FILE:-/tmp/hypr-overview-bind.enable}"

key_leftalt=56
key_d=32
key_tab=15
key_escape=1

need() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "missing required command: $1" >&2
    exit 2
  fi
}

tap_key() {
  local key="$1"
  ydotool key "$key:1" "$key:0" >/dev/null
}

super_tap() {
  local key="$1"
  ydotool key "$key_leftalt:1" "$key:1" "$key:0" "$key_leftalt:0" >/dev/null
}

show_count() {
  if [[ ! -e "$trace_file" ]]; then
    echo 0
    return
  fi

  grep -c 'hyprwinview show' "$trace_file" || true
}

focus_count() {
  if [[ ! -e "$trace_file" ]]; then
    echo 0
    return
  fi

  grep -c 'focus_direction right' "$trace_file" || true
}

need ydotool

touch "$trace_enable_file"
trap 'rm -f "$trace_enable_file"' EXIT
: > "$trace_file"

reproduced=0
passed=0
inconclusive=0

for ((i = 1; i <= iterations; i++)); do
  tap_key "$key_escape"
  sleep "$settle_after_key"

  before_focus="$(focus_count)"
  before_show="$(show_count)"

  super_tap "$key_d"
  sleep "$wait_after_focus"

  after_focus="$(focus_count)"
  if (( after_focus <= before_focus )); then
    echo "iteration $i: inconclusive: Super+D did not hit focus_direction trace"
    ((inconclusive += 1))
    continue
  fi

  super_tap "$key_tab"
  sleep "$settle_after_key"
  after_first_show="$(show_count)"

  if (( after_first_show > before_show )); then
    echo "iteration $i: pass: first Super+Tab fired"
    ((passed += 1))
    tap_key "$key_escape"
    sleep "$settle_after_key"
    continue
  fi

  super_tap "$key_tab"
  sleep "$settle_after_key"
  after_second_show="$(show_count)"
  tap_key "$key_escape"
  sleep "$settle_after_key"

  if (( after_second_show > after_first_show )); then
    echo "iteration $i: reproduced: first Super+Tab missed, second fired"
    ((reproduced += 1))
  else
    echo "iteration $i: inconclusive: neither Super+Tab fired"
    ((inconclusive += 1))
  fi
done

echo "summary: reproduced=$reproduced passed=$passed inconclusive=$inconclusive wait_after_focus=${wait_after_focus}s"

if (( reproduced > 0 )); then
  exit 1
fi

if (( inconclusive > 0 && passed == 0 )); then
  exit 2
fi

exit 0
