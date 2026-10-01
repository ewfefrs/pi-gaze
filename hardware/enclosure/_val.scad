include <src/params.scad>
include <src/lib.scad>
include <src/box.scad>
difference(){
  union(){
    color([1,0.85,0.1,0.9]) box_body();
    color([1,0.55,0.55,0.9]) box_lid();
    color([0.15,0.45,1]) translate([0,-9.8,48]) rotate([0,90,0]) import("C:/Users/bronl/Downloads/RASPBERRY_PI_5_WITH_ACTIVE_COOLER.stl", convexity=10);
  }
  translate([2,-60,-20]) cube([60,120,160]);   // убрать X>2 -> видно поперечный разрез Y-Z
}
