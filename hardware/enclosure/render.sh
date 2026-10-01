#!/bin/bash
OSC="/c/Program Files/OpenSCAD/openscad.exe"
SRC="src/pigaze.scad"; mkdir -p out
r(){ "$OSC" -o "out/$1.png" --imgsize=$4 --autocenter --viewall --camera=0,0,0,$3 --colorscheme=Tomorrow -D "PART=\"$2\"" "$SRC" >/dev/null 2>&1; }
r box_front    box     "58,0,180,0"  1000,1150
r box_left     box     "78,0,240,0"  1100,1000
r box_bottom   box     "125,0,185,0" 1200,700
r box_ext      box     "62,0,35,0"   1100,1000
r lid          box_lid "0,0,0,0"     950,1100
r bar_front    bar     "72,0,200,0"  1250,700
r bar_back     bar     "62,0,20,0"   1250,700
r clamp_iso    clamp   "68,0,200,0"  1050,900
r asm          all     "68,0,215,0"  1050,1200
ls -la out/*.png | awk '{print $NF, $5}'
