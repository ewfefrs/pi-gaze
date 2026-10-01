// ============================================================================
//  Pi-Gaze / bar.scad — оптический бар (камера + ИК), сидит на шарнире тисков
//
//  Светозащита: камера и каждый ИК-диод в СВОИХ отсеках. Перегородка —
//  боковая стенка центрального прилива, теперь НА ВСЮ ВЫСОТУ: полости крыльев
//  режутся только СНАРУЖИ прилива. (Раньше общая полость крыльев срезала
//  стенки/баффлы ниже Z 15.5 — ровно на высоте диодов, — а над крыльями
//  баффлы торчали наружу 2-мм плавниками.) Диод — в глухом колодце с малой
//  апертурой. Камера базируется по кромкам кармана.
//
//  Шлейф: одна центральная щель в полу сразу за камерой (разъём Camera Module 3
//  на нижней кромке платы сзади). Кабель Pi 5 (22->15 пин) шириной 16 мм уходит
//  вниз, в паз тисков. Нижние прижимные площадки крышки разведены в стороны,
//  чтобы шлейф шёл между ними.
// ============================================================================

BAR_L   = 118;                       // длина
BAR_D   = 18;                        // глубина (Y)
WING_H  = 18;                        // высота "крыльев"
BOSS_W  = CAM_X + 2*WALL + 9;        // 39 — центральный прилив под камеру
BOSS_H  = CAM_Z + 2*WALL + 3;        // 32
FRONT_Y = -BAR_D/2;                  // грань к лицу
BACK_Y  =  BAR_D/2;                  // грань стыка с крышкой
CAM_CZ  = BOSS_H/2;                  // высота оси камеры
CAM_BACK = FRONT_Y + WALL + CAM_T;   // 5: задняя грань камеры (её прижимает крышка)

HINGE_R = 7; HINGE_W = 10; FORK_GAP = HINGE_W + 0.8; FORK_KW = 8;
FORK_BLK_D = 9;                      // перемычки щёк к полу (уже прежних 10.5 -> не задевают шлейф)
IRW_X    = BOSS_W/2 - WALL/2;        // ось отверстий под провода ИК — по центру стенки прилива

// центральная щель шлейфа: между задней гранью камеры и пластиной-локатором крышки
BAR_RIB_W  = 16.4;
BAR_RIB_Y0 = CAM_BACK + 0.3;         // 5.3
BAR_RIB_Y1 = BACK_Y - 1.8;           // 7.2

// магниты: 4, симметрично
function bar_mag_x() = [-(BAR_L/2-10), -(BOSS_W/2+11), BOSS_W/2+11, BAR_L/2-10];

// вилка шарнира: в правой щеке — гайка M3 в кармане (затяжка = фиксация угла)
module hinge_fork(){
  difference(){
    union(){
      for(sx=[-1,1]) translate([sx*(FORK_GAP/2+FORK_KW/2),0,-HINGE_R])
        rotate([0,90,0]) cylinder(r=HINGE_R,h=FORK_KW,center=true);
      for(sx=[-1,1]) translate([sx*(FORK_GAP/2+FORK_KW/2),0,-HINGE_R/2+1])
        cube([FORK_KW,FORK_BLK_D,HINGE_R+2],center=true);
    }
    translate([-(FORK_GAP/2+FORK_KW+1),0,-HINGE_R]) rotate([0,90,0]) cylinder(d=3.4,h=FORK_KW+2);
    translate([0,0,-HINGE_R]) rotate([0,90,0]) cylinder(d=3.4,h=FORK_GAP+1,center=true);
    translate([FORK_GAP/2+FORK_KW/2, 0, -HINGE_R]) rotate([0,90,0]){
      cylinder(d=SCREW_M3_D, h=FORK_KW+4, center=true);                                          // проход винта
      translate([0,0,FORK_KW/2-NUT_M3_TH]) cylinder(d=(NUT_M3_AF+NUT_FIT)/cos(30), h=NUT_M3_TH+0.4, $fn=6); // гекс-карман
    }
  }
}

module bar_body(){
  difference(){
    union(){
      // --- корпус с выбранными полостями (полость режется ДО добавления столбиков магнитов)
      difference(){
        union(){
          rbox(BAR_L, BAR_D, WING_H, 3);                 // крылья
          rbox(BOSS_W, BAR_D, BOSS_H, 3);                // прилив под камеру
        }
        // полости крыльев — ТОЛЬКО снаружи прилива (стенка прилива = светоперегородка на всю высоту)
        for(sx=[-1,1]) translate([sx>0 ? BOSS_W/2 : -(BAR_L/2-WALL), FRONT_Y+WALL, WALL])
          cube([BAR_L/2-WALL-BOSS_W/2, BAR_D, WING_H-2*WALL]);
        // отсек камеры (открыт назад, к крышке)
        translate([-(BOSS_W/2-WALL), FRONT_Y+WALL, WALL]) cube([BOSS_W-2*WALL, BAR_D, BOSS_H-2*WALL]);
        // карман камеры: базирование по кромкам платы
        translate([-(CAM_X+CAM_FIT)/2, FRONT_Y+WALL, CAM_CZ-(CAM_Z+CAM_FIT)/2])
          cube([CAM_X+CAM_FIT, CAM_T+1.5, CAM_Z+CAM_FIT]);
        // окно объектива сквозь переднюю стенку (плоская грань -> печать оптикой вниз)
        translate([0, FRONT_Y-1, CAM_CZ]) rotate([-90,0,0]) cylinder(d=CAM_LENS_D, h=WALL+2);
        // ИК: глухой колодец изнутри + малая апертура наружу + карман резистора
        for(sx=[-1,1]){
          translate([sx*IR_SPACING/2, FRONT_Y+WALL+0.01, WING_H/2]) rotate([-90,0,0]) cylinder(d=IR_WELL_D, h=7);
          translate([sx*IR_SPACING/2, FRONT_Y-1,        WING_H/2]) rotate([-90,0,0]) cylinder(d=IR_APER_D, h=WALL+2);
          translate([sx*(IR_SPACING/2)+4, FRONT_Y+WALL, WING_H/2-5.5]) cube([10,4,3]);
          // провода ИК: сквозь стенку прилива (центр отверстия — по центру стенки)
          translate([sx*IRW_X, FRONT_Y+WALL+4, WING_H/2]) rotate([0,90,0]) cylinder(d=3.5,h=WALL+3,center=true);
        }
        // центральная щель шлейфа (+ провода ИК) в полу, сразу за камерой
        translate([-BAR_RIB_W/2, BAR_RIB_Y0, -1]) cube([BAR_RIB_W, BAR_RIB_Y1-BAR_RIB_Y0, WALL+2]);
      }
      // --- столбики магнитов у задней кромки (добавляются ПОСЛЕ полости)
      for(x=bar_mag_x()) translate([x, BACK_Y-2, WING_H/2]) cube([9,4,WING_H-2*WALL], center=true);
    }
    // --- карманы магнитов
    for(x=bar_mag_x()) translate([x,0,WING_H/2]) mag_pocket_pY(BACK_Y);
  }
  hinge_fork();
}

// крышка бара: моделируется НА МЕСТЕ (у задней грани бара). ПОВТОРЯЕТ ПРОФИЛЬ бара.
module bar_lid(){
  seg_l = BAR_L/2 - WALL - BOSS_W/2 - FIT;       // бортик крыла: только снаружи прилива
  difference(){
    union(){
      // --- плита по профилю бара: крылья + центральный прилив под камеру
      translate([0,BACK_Y+LID_TH/2,0]){
        rbox(BAR_L,  LID_TH, WING_H, 3);
        rbox(BOSS_W, LID_TH, BOSS_H, 3);
      }
      // --- локатор-бортик в полости: 2 сегмента в крыльях + 1 в отсеке камеры
      // (заходят в плиту на 0.5 мм: раньше только КАСАЛИСЬ её по Y=9 и были отдельными телами)
      translate([0,BACK_Y-0.65,WALL]){
        for(sx=[-1,1]) translate([sx*(BOSS_W/2 + FIT/2 + seg_l/2), 0, 0])
          rbox(seg_l, 2.3, WING_H-2*WALL-FIT, 1);
        rbox(BOSS_W-2*WALL-FIT, 2.3, BOSS_H-2*WALL-FIT, 2);
      }
      // --- площадки прижима камеры: верхние по углам, нижние разведены наружу (шлейф между ними)
      for(sx=[-1,1]){
        translate([sx*(CAM_X/2-3),   (CAM_BACK+BACK_Y+0.5)/2, CAM_CZ+(CAM_Z/2-3)]) cube([6,   BACK_Y+0.5-CAM_BACK, 6], center=true);
        translate([sx*(CAM_X/2-1.6), (CAM_BACK+BACK_Y+0.5)/2, CAM_CZ-(CAM_Z/2-3)]) cube([3.2, BACK_Y+0.5-CAM_BACK, 6], center=true);
      }
    }
    for(x=bar_mag_x()) translate([x,0,WING_H/2]) mag_pocket_nY(BACK_Y);
    // ВЫРЕЗЫ в бортике под столбики магнитов бара
    for(x=bar_mag_x()) translate([x, BACK_Y-0.9, WING_H/2]) cube([9+2*FIT, 1.9, WING_H-2*WALL+2], center=true);
  }
}
