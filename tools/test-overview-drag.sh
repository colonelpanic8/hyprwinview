#!/usr/bin/env bash
set -euo pipefail

# Drags a tiled window around the live workspace overview and checks, from
# screenshots, that the dragged preview stays visible at its tiled size.
#
# Spawns a magenta window (the one dragged) and a green neighbour on an empty
# workspace, opens the overview there, presses the drag button over the magenta
# tile with ydotool, and screenshots the output at points along the drag path.
# Each screenshot's magenta bounding box is compared against the tile measured
# before the press. Runs against the real session: it moves the pointer and
# switches workspaces, and closes its windows on exit.

ws="${DRAG_TEST_WORKSPACE:-}"
out="${DRAG_TEST_OUT:-$(mktemp -d /tmp/overview-drag-test.XXXXXX)}"
steps="${DRAG_TEST_STEPS:-12}"
tolerance="${DRAG_TEST_TOLERANCE:-0.06}"
record="${DRAG_TEST_RECORD:-0}"
shot_scale=0.25
class="overview-drag-test"
export YDOTOOL_SOCKET="${YDOTOOL_SOCKET:-/run/ydotoold/socket}"

need() {
  command -v "$1" >/dev/null 2>&1 || { echo "missing required command: $1" >&2; exit 2; }
}
need hyprctl
need ydotool
need grim
need alacritty
need python3

mkdir -p "$out"
log() { echo "[$(date +%T.%N | cut -c1-12)] $*" | tee -a "$out/log.txt" >&2; }

monitor_json="$(hyprctl monitors -j | python3 -c 'import json,sys; print(json.dumps(next(m for m in json.load(sys.stdin) if m["focused"])))')"
monitor="$(python3 -c 'import json,sys; print(json.loads(sys.argv[1])["name"])' "$monitor_json")"
read -r mon_x mon_y mon_w mon_h < <(python3 -c 'import json,sys; m=json.loads(sys.argv[1]); s=m["scale"]; print(m["x"], m["y"], round(m["width"]/s), round(m["height"]/s))' "$monitor_json")
origin_ws="$(hyprctl activeworkspace -j | python3 -c 'import json,sys; print(json.load(sys.stdin)["id"])')"

if [[ -z "$ws" ]]; then
  ws="$(hyprctl workspaces -j | python3 -c '
import json, sys
used = {w.get("id") for w in json.load(sys.stdin) if w["windows"] > 0}
print(next(i for i in range(9, 0, -1) if i not in used))')"
fi

lua() { hyprctl eval "$1" >/dev/null; }
toggle_overview() { lua 'hl.plugin.hyprtasking.toggle("cursor")'; }

cleanup() {
  set +e
  ydotool click 0x81 >/dev/null 2>&1
  if [[ "${overview_open:-0}" == 1 ]]; then toggle_overview; sleep 0.5; fi
  hyprctl clients -j | python3 -c "
import json, sys
for c in json.load(sys.stdin):
    if c['class'] == '$class': print(c['pid'])" | xargs -r kill
  lua "hl.dispatch(hl.dsp.focus({ workspace = \"$origin_ws\" }))"
  [[ -n "${rec_pid:-}" ]] && kill -INT "$rec_pid" && wait "$rec_pid"
}
trap cleanup EXIT

spawn() {
  local name="$1" color="$2"
  lua "hl.exec_cmd(\"alacritty --class $class --title $name -o window.opacity=1.0 -o 'colors.primary.background=\\\"$color\\\"' -e sleep 3600\", { workspace = \"$ws silent\" })"
}

cursor() { hyprctl cursorpos | tr -d ,; }

# Relative ydotool steps with cursorpos feedback, so pointer acceleration does
# not matter and the drag produces a stream of motion events.
move_to() {
  local tx="$1" ty="$2" max_step="${3:-12}" cx cy dx dy i
  for ((i = 0; i < 400; i++)); do
    read -r cx cy < <(cursor)
    dx=$((tx - cx)); dy=$((ty - cy))
    if ((${dx#-} <= 2 && ${dy#-} <= 2)); then return 0; fi
    ((dx > max_step)) && dx=$max_step; ((dx < -max_step)) && dx=-$max_step
    ((dy > max_step)) && dy=$max_step; ((dy < -max_step)) && dy=-$max_step
    ydotool mousemove -x "$dx" -y "$dy" >/dev/null
    sleep 0.01
  done
  log "move_to $tx,$ty did not converge"
  return 1
}

shot() { grim -o "$monitor" -s "$shot_scale" -t ppm "$out/$1.ppm"; }

# Prints "x y w h" (logical px) of the magenta region, or "none".
magenta_box() {
  python3 - "$out/$1.ppm" "$shot_scale" "$mon_x" "$mon_y" <<'PY'
import sys
path, scale, ox, oy = sys.argv[1], float(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
data = open(path, "rb").read()
parts = data.split(maxsplit=4)
w, h = int(parts[1]), int(parts[2])
px = parts[4]
xs, ys = [], []
for y in range(h):
    row = px[y * w * 3:(y + 1) * w * 3]
    for x in range(w):
        r, g, b = row[3 * x], row[3 * x + 1], row[3 * x + 2]
        if r > 110 and b > 110 and g * 2.5 < min(r, b):
            xs.append(x); ys.append(y)
if len(xs) < 20:
    print("none")
else:
    f = 1 / scale
    print(round(min(xs) * f) + ox, round(min(ys) * f) + oy,
          round((max(xs) - min(xs) + 1) * f), round((max(ys) - min(ys) + 1) * f))
PY
}

log "output: $out (monitor $monitor ${mon_w}x${mon_h}, workspace $ws)"
spawn drag-test-a "#ff00ff"
sleep 0.6
spawn drag-test-b "#00c000"
sleep 1.5
lua "hl.dispatch(hl.dsp.focus({ workspace = \"$ws\" }))"
sleep 0.5

if [[ "$record" == 1 ]]; then
  if command -v wf-recorder >/dev/null 2>&1; then
    wfr=wf-recorder
  else
    wfr="$(nix build --no-link --print-out-paths nixpkgs#wf-recorder)/bin/wf-recorder"
  fi
  "$wfr" -o "$monitor" -f "$out/drag.mp4" -y >/dev/null 2>&1 &
  rec_pid=$!
  sleep 1
fi

toggle_overview
overview_open=1
sleep 1
shot 00-overview
read -r tx ty tw th < <(magenta_box 00-overview) || true
if [[ "$tx" == none || -z "${tw:-}" ]] || ((tw > mon_w / 2)); then
  log "could not find the magenta tile in the overview (got: $tx ${ty:-} ${tw:-} ${th:-})"
  exit 2
fi
log "tile: ${tw}x${th} at $tx,$ty"

grab_x=$((tx + tw / 2)); grab_y=$((ty + th / 2))
move_to "$grab_x" "$grab_y"
sleep 0.3
ydotool click 0x41 >/dev/null # right button down (plugin:hyprtasking:drag_button = 0x111)
sleep 0.4
shot 01-pressed

# Path: down one tile row, across, and back to the start.
row=$((mon_h / 3 * (grab_y > mon_y + mon_h / 2 ? -1 : 1)))
col=$((mon_w / 3 * (grab_x > mon_x + mon_w / 2 ? -1 : 1)))
path=()
for ((i = 1; i <= steps; i++)); do
  if ((i <= steps / 3)); then
    path+=("$grab_x $((grab_y + row * i * 3 / steps))")
  elif ((i <= 2 * steps / 3)); then
    k=$((i - steps / 3))
    path+=("$((grab_x + col * k * 3 / steps)) $((grab_y + row))")
  else
    k=$((i - 2 * steps / 3))
    last="${path[-1]}"; lx="${last% *}"; ly="${last#* }"
    path+=("$((lx + (grab_x - lx) * k * 3 / steps)) $((ly + (grab_y - ly) * k * 3 / steps))")
  fi
done

seen=0 missing=0 wrong=0
results="$out/results.txt"
: > "$results"
for ((i = 0; i < ${#path[@]}; i++)); do
  read -r px py <<<"${path[$i]}"
  move_to "$px" "$py"
  sleep 0.15
  name="$(printf '%02d-step' $((i + 2)))"
  shot "$name"
  read -r bx by bw bh < <(magenta_box "$name") || true
  if [[ "$bx" == none ]]; then
    verdict=missing; ((missing += 1))
  else
    verdict="$(python3 -c "
bw, bh, tw, th, tol = $bw, $bh, $tw, $th, $tolerance
print('ok' if abs(bw / tw - 1) <= tol and abs(bh / th - 1) <= tol else 'size')")"
    if [[ "$verdict" == ok ]]; then ((seen += 1)); else ((wrong += 1)); fi
  fi
  echo "$name cursor=$px,$py preview=${bx}${by:+ $by $bw $bh} expect=${tw}x${th} $verdict" | tee -a "$results" >&2
done

ydotool click 0x81 >/dev/null
sleep 0.8
shot 99-dropped
toggle_overview
overview_open=0
sleep 0.5

log "summary: ok=$seen wrong_size=$wrong missing=$missing of ${#path[@]} (tile ${tw}x${th})"
if ((missing > 0 || wrong > 0)); then exit 1; fi
