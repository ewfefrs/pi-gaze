include <src/params.scad>
include <src/lib.scad>
include <src/box.scad>
intersection(){ box_body(); translate([0,-9.8,48]) rotate([0,90,0]) import("C:/Users/bronl/Downloads/RASPBERRY_PI_5_WITH_ACTIVE_COOLER.stl", convexity=10); }
