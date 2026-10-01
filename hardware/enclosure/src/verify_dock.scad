// Dock-mating check: modified ready-made case hung on the EXISTING clamp dock.
include <params.scad>
include <lib.scad>
include <clamp.scad>

// ghost monitor for context (thin, so it doesn't occlude the case)
%translate([-95,-REACH/2+JAW_T,-210]) cube([190,10,210]);

// the reused vice/clamp (тиски)
clamp();

// modified case docked: case X->clampZ(up), case Y->clampX(width), case Z(cover)->clampY(out)
// floor(caseZ=-2.5)->clampY=28 (dock boss face); hole(caseX=44)->clampZ=-49 (dock)
color([0.25,0.55,0.9])
  translate([0, 30.5, -93])
    multmatrix([[0,1,0,0],[0,0,1,0],[1,0,0,0],[0,0,0,1]])
      import("C:/Users/bronl/Desktop/pi-gaze/hardware/mount/case_mod_v3.stl");
