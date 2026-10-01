#!/bin/bash
# Pi-Gaze: full rebuild + COUNTER-MODEL.
#  1) modify the ready-made case/cover (CadQuery + Manifold)  -> mount/case_pigaze.stl, cover_pigaze.stl
#  2) export every printable OpenSCAD part                      -> enclosure/stl/pigaze_*.stl
#  3) export assembled poses for the checker                    -> enclosure/out/pose/
#  4) run the counter-model                                     -> check_report.txt
set -eo pipefail
cd "$(dirname "$0")"
OSC="/c/Program Files/OpenSCAD/openscad.exe"
PY=./mount/.venv/Scripts/python.exe

echo "== 1) case + cover"
(cd mount && ../$PY case_mod.py 2>&1 | grep -v "cannot open font")

echo "== 2) OpenSCAD parts"
mkdir -p enclosure/stl enclosure/out/pose
for p in clamp ribbon_cover pad foot knob bar bar_lid; do
  "$OSC" -o enclosure/stl/pigaze_$p.stl -D "PART=\"$p\"" enclosure/src/pigaze.scad 2>&1 | grep -iE "warning|error" || true
done

echo "== 3) poses"
cp enclosure/stl/pigaze_clamp.stl enclosure/out/pose/pose_clamp.stl
for p in pose_ribbon_cover pose_pad pose_foot; do
  "$OSC" -o enclosure/out/pose/$p.stl -D "PART=\"$p\"" enclosure/src/pigaze.scad 2>&1 | grep -iE "warning|error" || true
done
for y in 31 45; do
  "$OSC" -o enclosure/out/pose/pose_knob_y$y.stl -D 'PART="pose_knob"' -D "KNOB_Y=$y" enclosure/src/pigaze.scad 2>&1 | grep -iE "warning|error" || true
done

echo "== 4) counter-model"
$PY check_pigaze.py 2>&1 | grep -v -E "cannot open font|RuntimeWarning|center_mass|inverse_denominator|barycentric" | tee check_report.txt
