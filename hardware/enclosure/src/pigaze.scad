// ============================================================================
//  Pi-Gaze — айтрекер. ГЛАВНЫЙ ФАЙЛ сборки.
//
//  Печатные детали (OpenSCAD):   openscad -D 'PART="clamp"' pigaze.scad
//    clamp | ribbon_cover | pad | foot | knob | bar | bar_lid
//  Готовый кейс RPi5 + крышка модифицируются отдельно (CadQuery+Manifold):
//    hardware/mount/case_mod.py -> case_pigaze.stl, cover_pigaze.stl
//  Позы для контр-модели:  PART="pose_bar" | "pose_barlid" | "pose_knob" |
//    "pose_foot" | "pose_pad" | "pose_ribbon_cover"
//  PART="all" — собранный вид (монитор и реальный Pi 5 — призраки).
// ============================================================================
include <params.scad>
include <lib.scad>
include <bar.scad>
include <clamp.scad>

PART      = "all";
GHOST     = true;
TILT      = -38;       // наклон бара в собранном виде (рабочий диапазон -20..-48)
MONITOR_T = 15;        // толщина верхней кромки монитора (призрак + поза пятки)
KNOB_Y    = REACH/2 + 7;

CASE_STL  = "../../mount/case_pigaze.stl";
COVER_STL = "../../mount/cover_pigaze.stl";
PI_STL    = "C:/Users/bronl/Downloads/RASPBERRY_PI_5_WITH_ACTIVE_COOLER.stl";
// ДОЛЖНО совпадать с hardware/mount/case_mod.py (HANG_T, PI_T) — сверяется контр-моделью
HANG_T = [[0,1,0,3.5],[0,0,-1,40.5],[-1,0,0,-94],[0,0,0,1]];
PI_T   = [[1,0,0,0.1],[0,0,-1,0],[0,1,0,2.88],[0,0,0,1]];

module pose_bar()    { translate([HINGE_X,HY,PIV_Z]) rotate([TILT,0,0]) translate([0,0,HR]) bar_body(); }
module pose_barlid() { translate([HINGE_X,HY,PIV_Z]) rotate([TILT,0,0]) translate([0,0,HR]) bar_lid(); }
module pose_knob()   { translate([0, KNOB_Y, SCREW_Z]) rotate([-90,0,0]) clamp_knob(); }
module pose_foot()   { translate([0, -REACH/2+JAW_T+MONITOR_T+2, SCREW_Z-9]) clamp_foot(); }
module pose_pad()    { translate([0, -REACH/2+JAW_T-0.7, -0.5-(LIP_H-4.4)/2]) clamp_pad(); }
module pose_case()   { multmatrix(HANG_T) import(CASE_STL, convexity=10); }
module pose_cover()  { multmatrix(HANG_T) import(COVER_STL, convexity=10); }
module pose_pi()     { multmatrix(HANG_T) multmatrix(PI_T) import(PI_STL, convexity=10); }
module ghost_monitor(){ translate([-160, -REACH/2+JAW_T, -300]) cube([320, MONITOR_T, 300]); }

if      (PART=="clamp")        clamp();
else if (PART=="ribbon_cover") translate([0,0,-(TOP_T-COVER_T)]) ribbon_cover();
else if (PART=="pad")          clamp_pad();
else if (PART=="foot")         clamp_foot();
else if (PART=="knob")         clamp_knob();
else if (PART=="bar")          bar_body();
else if (PART=="bar_lid")      bar_lid();
else if (PART=="pose_bar")     pose_bar();
else if (PART=="pose_barlid")  pose_barlid();
else if (PART=="pose_knob")    pose_knob();
else if (PART=="pose_foot")    pose_foot();
else if (PART=="pose_pad")     pose_pad();
else if (PART=="pose_ribbon_cover") ribbon_cover();
else {
  if (GHOST) { %ghost_monitor(); color([0.15,0.5,0.2]) pose_pi(); }
  color([0.25,0.45,0.75]) { clamp(); pose_knob(); pose_bar(); }
  color([0.95,0.6,0.15])  { ribbon_cover(); pose_barlid(); }
  color([0.2,0.2,0.2])    { pose_pad(); pose_foot(); }
  color([0.85,0.85,0.88]) pose_case();
  color([0.7,0.72,0.75])  pose_cover();
}
