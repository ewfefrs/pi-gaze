#!/bin/bash
# Pi-Gaze: export printable OpenSCAD parts. For the full pipeline (case mod + counter-model) run ../check.sh
set -eo pipefail
cd "$(dirname "$0")"
OSC="/c/Program Files/OpenSCAD/openscad.exe"
mkdir -p stl
for p in clamp ribbon_cover pad foot knob bar bar_lid; do
  "$OSC" -o stl/pigaze_$p.stl -D "PART=\"$p\"" src/pigaze.scad 2>&1 | grep -iE "warning|error" || true
  echo "exported stl/pigaze_$p.stl"
done
