// ============================================================================
//  Pi-Gaze / lib.scad — примитивы. Ничего проектного, только геометрия.
// ============================================================================
include <params.scad>

// Скруглённый прямоугольник (2D)
module rrect(w,h,r){ minkowski(){ square([w-2*r,h-2*r],center=true); circle(r=r); } }

// Коробка со скруглёнными вертикальными рёбрами: центр в XY, z = 0..h
// ВАЖНО: радиус ограничивается габаритом. Иначе при w < 2*r hull выворачивается
// наизнанку и деталь РАЗДУВАЕТСЯ (тонкая пластина 3.2 превращалась в 12.8).
module rbox(l,w,h,r=3){
  rr = min(r, l/2-0.01, w/2-0.01);
  hull() for(x=[rr-l/2,l/2-rr]) for(y=[rr-w/2,w/2-rr])
    translate([x,y,0]) cylinder(r=rr,h=h);
}
// То же, но центр по всем осям
module rbox3(l,w,h,r=3){ translate([0,0,-h/2]) rbox(l,w,h,r); }

// Выдавливание вдоль +Y (локально): удобно для стенок корпуса
module extrudeY(len){ rotate([-90,0,0]) linear_extrude(len) children(); }

// ---------------------------------------------------------------- термовтулки
// Отверстие под запрессовку с заходным конусом. Ось +Z, устье на z=0.
module insert_hole(d,l){
  union(){
    cylinder(d=d, h=l+INS_SLACK);
    translate([0,0,-0.01]) cylinder(d1=d+1.2, d2=d, h=1.0);
  }
}
// Бобышка под втулку: цилиндр высотой h, втулка запрессовывается с ТОРЦА (z=h)
module boss(d_ins,l_ins,h){
  difference(){
    cylinder(d=d_ins+2*BOSS_WALL, h=h);
    translate([0,0,h-l_ins-INS_SLACK]) insert_hole(d_ins,l_ins);
  }
}

// ------------------------------------------------------------------- магниты
// Карман под магнит, открытый в +Y, с потолком MAG_CEIL со стороны faceY.
// Печатается стыковой гранью вверх -> на паузе кладём магнит, слои замуровывают.
module mag_pocket_pY(faceY){
  translate([0,faceY-MAG_CEIL,0]) rotate([90,0,0]) cylinder(d=MAG_PD,h=MAG_H);
}
// То же, зеркально: потолок со стороны -Y
module mag_pocket_nY(faceY){
  translate([0,faceY+MAG_CEIL,0]) rotate([-90,0,0]) cylinder(d=MAG_PD,h=MAG_H);
}

// ------------------------------------------------------------- проёмы разъёмов
// Прямоугольный проём с фаской-заходом по НАРУЖНОЙ грани.
// Локально: сквозь по +Z, наружная грань на z=thick. w вдоль X, h вдоль Y.
module port_slot(w,h,thick){
  translate([0,0,-0.1]) linear_extrude(thick+0.2) square([w,h],center=true);
  translate([0,0,thick-CHAM])
    linear_extrude(CHAM+0.1, scale=[(w+2*CHAM)/w,(h+2*CHAM)/h]) square([w,h],center=true);
}

// ---------------------------------------------------------------- вентиляция
// Сквозная щель в стенке, нормаль которой = ось X. sx = -1 (левая) / +1 (правая).
// Куб центрируется НА стенке -> гарантированно прорезает насквозь.
module vent_x(sx, xface, ycen, zcen, len, thickness=2.4){
  translate([sx*xface, ycen, zcen]) cube([WALL*3, len, thickness], center=true);
}

// ------------------------------------------------------- гайка в кармане (nut-trap)
// РЕЖУЩИЙ инструмент (вычитается). Ось винта = +Z.
//  - сквозной проход под винт по всей высоте (обе стороны);
//  - шестигранный карман под гайку у -Z конца, ОТКРЫТ в -Z (туда вставляется гайка).
// Ориентируй rotate/translate так, чтобы карман открывался на доступную грань
// (или замуровывался соседней деталью — тогда гайка не выпадет).
module nut_trap_cut(af=NUT_M3_AF, nt=NUT_M3_TH, sd=SCREW_M3_D, thru=20){
  translate([0,0,-thru]) cylinder(d=sd, h=2*thru);                               // проход винта насквозь
  // гекс-карман: across-flats = af+NUT_FIT  =>  d(across-corners) = (af+NUT_FIT)/cos(30)
  translate([0,0,-nt]) cylinder(d=(af+NUT_FIT)/cos(30), h=nt+0.02, $fn=6);
}

// Бобышка под термовтулку, ось +Y (устье втулки смотрит в +Y). Для крепления платы.
module boss_pinsertY(y0, h, d_ins, l_ins){
  translate([0, y0, 0]) rotate([-90,0,0]) difference(){
    cylinder(d=d_ins+2*BOSS_WALL, h=h);
    translate([0,0,h-l_ins-INS_SLACK]) insert_hole(d_ins, l_ins);
  }
}
