// ============================================================================
//  Pi-Gaze / check.scad — КОНТР-МОДЕЛЬ (falsification harness)
//  Задача: ДОКАЗАТЬ, что корпус плохой. Ставим ghost-габариты реальных
//  компонентов в их СБОРОЧНЫЕ позиции и пересекаем с пластиком.
//  Любое НЕПУСТОЕ пересечение = коллизия = провал.
//  Рендер:  openscad -o out.stl -D 'CHECK="cooler_box"' src/check.scad
//  Пустой результат ("top level object is empty") = PASS для данной пары.
// ============================================================================
include <params.scad>
include <lib.scad>
include <box.scad>
include <bar.scad>
include <clamp.scad>

CHECK = "none";

// ------- ghost-габариты реальных компонентов (в координатах box) -----------
module g_pcb(){ translate([-PCB_X/2, BACK_IN+STAND_H, PZ0]) cube([PCB_X, PCB_T, PCB_Z]); }

// Active Cooler: блок на компонентной стороне (Y от PY вверх), центр по SoC (~центр платы)
CL_CW=42.5; CL_CD=63.5; CL_CH=13.7; // РЕАЛ (офиц. datasheet Active Cooler)
module g_cooler(){ translate([-CL_CW/2, PY, PZ0+PCB_Z/2-CL_CD/2]) cube([CL_CW, CL_CH, CL_CD]); }

// Тела разъёмов (ИЗМЕРЕНЫ на реальной STL [S3D]), пересекающие свою стенку
BODY_LAN =[15.90,13.68]; BODY_USB=[13.25,15.80]; BODY_USBC=[8.94,3.16]; BODY_HDMI=[6.50,2.88];
module cbody_bottom(cx,w,h){ translate([cx-w/2, PY, -3]) cube([w, h, PZ0+3]); }
module cbody_left(cz,w,h){ translate([-BOX_X/2-3, PY, cz-w/2]) cube([WALL+5, h, w]); }
module cbody_right(cz,w,h){ translate([BOX_X/2-WALL-2, PY, cz-w/2]) cube([WALL+5, h, w]); }

// CH9329 тело в отсеке
module g_ch(){ translate([0-CH_L/2, BACK_IN+1, CH_Z-CH_W/2]) cube([CH_L, CH_T, CH_W]); }

// столбики опор платы (как в box_body)
module g_posts(){ for(x=[-1,1]) for(z=[PZ0+6, PZ0+PCB_Z-6])
  translate([x*(PCB_X/2-6), BACK_IN, z]) rotate([-90,0,0]) cylinder(d=7,h=STAND_H); }

// камера ghost в баре (локальные коорд. бара)
module g_cam(){ translate([-CAM_X/2, FRONT_Y+WALL, CAM_CZ-CAM_Z/2]) cube([CAM_X, CAM_T, CAM_Z]); }
module g_lens(){ translate([0,FRONT_Y-HOOD_H-2,CAM_CZ]) rotate([-90,0,0]) cylinder(d=CAM_LENS_D, h=HOOD_H+WALL+3); }

// ============================ ПРОВЕРКИ ======================================
if      (CHECK=="cooler_box")  intersection(){ g_cooler(); box_body(); }
else if (CHECK=="cooler_lid")  intersection(){ g_cooler(); box_lid(); }
else if (CHECK=="pcb_shell")   intersection(){ g_pcb(); difference(){ box_body();
        // вычесть намеренные контакты (опоры/полки/клипсы/док-пятаки) — оставить только стенки
        g_posts();
        for(x=[7.6,-10.05]){ translate([x-1.5, BACK_IN, PZ0-3]) cube([3, STAND_H+PCB_T, 3]);
          translate([x-1.5, BACK_IN+STAND_H+PCB_T-0.4, PZ0-3]) cube([3, 1.4, 7]); }
        for(p=board_screws()) translate([p[0], BACK_IN, p[1]]) rotate([-90,0,0])
          cylinder(d=INS_M25_D+2*BOSS_WALL+0.3, h=STAND_H+0.5);   // винтовые бобышки платы
      } }
else if (CHECK=="lan")   intersection(){ cbody_bottom(-(PX0+P_BOTTOM[0]), BODY_LAN[0], BODY_LAN[1]); box_body(); }
else if (CHECK=="usb1")  intersection(){ cbody_bottom(-(PX0+P_BOTTOM[1]), BODY_USB[0], BODY_USB[1]); box_body(); }
else if (CHECK=="usb2")  intersection(){ cbody_bottom(-(PX0+P_BOTTOM[2]), BODY_USB[0], BODY_USB[1]); box_body(); }
else if (CHECK=="usbc")  intersection(){ cbody_right(PZ0+P_LEFT[0], BODY_USBC[0], BODY_USBC[1]); box_body(); }
else if (CHECK=="hdmi0") intersection(){ cbody_right(PZ0+P_LEFT[1], BODY_HDMI[0], BODY_HDMI[1]); box_body(); }
else if (CHECK=="hdmi1") intersection(){ cbody_right(PZ0+P_LEFT[2], BODY_HDMI[0], BODY_HDMI[1]); box_body(); }
else if (CHECK=="ch_pcb")   intersection(){ g_ch(); g_pcb(); }
else if (CHECK=="ch_posts")  intersection(){ g_ch(); g_posts(); }
else if (CHECK=="cam_bar")  intersection(){ g_cam(); bar_body(); }
else if (CHECK=="lens_bar") intersection(){ g_lens(); bar_body(); }

// --- добавлено: проверка новых нижних опор против ВСЕХ тел разъёмов ---
module g_botsup(){ for(x=[7.6,-10.05]) translate([x-1.5, BACK_IN, PZ0-3]) cube([3, STAND_H+PCB_T+1, 7]); }
module g_bot_all(){ union(){
  cbody_bottom(-(PX0+P_BOTTOM[0]), BODY_LAN[0], BODY_LAN[1]);
  cbody_bottom(-(PX0+P_BOTTOM[1]), BODY_USB[0], BODY_USB[1]);
  cbody_bottom(-(PX0+P_BOTTOM[2]), BODY_USB[0], BODY_USB[1]); } }
if (CHECK=="botsup") intersection(){ g_botsup(); g_bot_all(); }

// --- посадка ТОЛКАТЕЛЯ в направляющую (шток+шляпка входят с зазором) ---
module g_plunger(){ translate([PWR_TX, PY+1.5, 91]) pwr_plunger(); }
if (CHECK=="plunger_fit") intersection(){ g_plunger(); box_body(); }   // пусто = входит

// --- диапазон наклона шарнира: бар при -48° не задевает тиски (рабочий ~-38°) ---
if (CHECK=="hinge_45") intersection(){
  translate([0,HY,PIV_Z]) rotate([-48,0,0]) translate([0,0,HR]) bar_body();
  clamp();
}

// --- СТРОГО: крышка бара ОБЯЗАНА накрыть отсек камеры выше крыльев ---
module g_cam_backgap(){ translate([-(BOSS_W/2-WALL), BACK_Y+0.2, WING_H+0.5])
  cube([BOSS_W-2*WALL, LID_TH-0.4, BOSS_H-WALL-WING_H-1]); }
if (CHECK=="lid_covers") difference(){ g_cam_backgap(); bar_lid(); }   // пусто = накрыто

// --- СТРОГО: крышка бара садится в бар без коллизии (бортик/площадки в полостях) ---
if (CHECK=="barlid_bar") intersection(){ bar_lid(); bar_body(); }

// --- добавлено: СБОРОЧНЫЕ проверки (позы из pigaze.scad) ---
if (CHECK=="asm_bar_clamp") intersection(){
  translate([0,HY,PIV_Z]) rotate([-38,0,0]) translate([0,0,HR]) bar_body();
  clamp();
}
if (CHECK=="asm_box_clamp") intersection(){
  translate([0, REACH/2+4+BOX_Y/2, DOCK_Z-DOCK_BOX_Z]) box_body();
  clamp();
}
