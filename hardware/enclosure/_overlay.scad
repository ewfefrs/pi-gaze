include <src/params.scad>
include <src/lib.scad>
include <src/box.scad>
color([1,0.85,0.1]) box_body();
color([0.15,0.45,1,0.5]) translate([0,-9.8,48]) rotate([0,90,0]) import("C:/Users/bronl/Downloads/RASPBERRY_PI_5_WITH_ACTIVE_COOLER.stl", convexity=10);
