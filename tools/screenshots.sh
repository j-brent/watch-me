#!/usr/bin/env bash
# Regenerates docs/screenshots/*.png headless (QQuickWindow::grabWindow via --grab).
# Usage: tools/screenshots.sh [build-dir]
set -euo pipefail
cd "$(dirname "$0")/.."
BIN="${1:-build}/watch-me"
OUT=docs/screenshots
mkdir -p "$OUT"
export QT_QPA_PLATFORM=offscreen
T0=1987-06-01T10:08:30

shot() {  # name crop(x,y,w,h|full) delay script
    local name=$1 crop=$2 delay=$3 script=$4
    "$BIN" --time "$T0" --grab-delay "$delay" --script "$script" --grab "$OUT/$name.png" 2>/dev/null
    if [ "$crop" != full ]; then
        python3 - "$OUT/$name.png" "$crop" <<'PY'
import sys
from PIL import Image
x, y, w, h = map(int, sys.argv[2].split(','))
Image.open(sys.argv[1]).crop((x, y, x + w, y + h)).save(sys.argv[1])
PY
    fi
}

W=0,0,500,480
shot watch-dead      $W 200 "wait:100"
shot watch-time      $W 200 "battery.inserted;wait:200"
shot watch-date      $W 200 "battery.inserted;d"
shot watch-update    $W 200 "battery.inserted;+c;wait:2300;-c;c;c;c;wait:120"
shot watch-alarm1    $W 200 "battery.inserted;alarm:1:07:15;a;d"
shot watch-stopwatch $W 700 "battery.inserted;a;a;a;a;b;wait:1500;d"
shot watch-beeping   $W 100 "battery.inserted;alarm:1:10:30;a;d;a;a;a;a;jump:1:1;wait:1300"
shot watch-beeptest  $W 200 "battery.inserted;+d;+b;wait:200"
echo "screenshots written to $OUT"
