include <src/params.scad>
include <src/lib.scad>
include <src/box.scad>
// корпус полупрозрачный, реальная плата сплошная -> видно, как разъёмы садятся в проёмы
color([0.6,0.6,0.62,0.22]) box_body();
color([0.1,0.5,0.9]) translate([0,-9.8,48]) rotate([0,90,0])
  import("C:/Users/bronl/Downloads/RASPBERRY_PI_5_WITH_ACTIVE_COOLER.stl", convexity=10);
