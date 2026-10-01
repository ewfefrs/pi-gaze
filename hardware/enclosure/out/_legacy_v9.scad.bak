// ============================================================
//  Pi-Gaze enclosure - v5   (единицы: мм)
//  Крепёж: ТЕРМОВТУЛКИ (heat-set).  Магниты: N42 диск 5x2, ВПАИВАЮТСЯ.
//
//  Детали для печати:
//    bar | bar_lid | cam_clip | clamp | pibox | pibox_lid | pad | foot
//  Рендер одной детали:  -D part="bar"
//
//  ========== ВПАЙКА МАГНИТОВ НА ПАУЗЕ ==========
//  Магнит 5x2. Над магнитом со стороны стыка оставлен потолок mag_ceil=0.4 мм:
//  ставишь паузу (M600 / "pause at height" в слайсере) на слое, где карман
//  открылся, кладёшь магнит, продолжаешь - следующие слои его замуровывают.
//  Полярность: все магниты в КОРПУСЕ одной стороной, в КРЫШКЕ - противоположной.
//  mag_ceil=0 -> карманы открытые, под клей.
//
//    деталь       ориентация на столе            ПАУЗА (M600) на Z
//    bar          окнами вниз                       ~13.6 мм
//    bar_lid      наружной гранью вниз              ~0.8 мм (корка тонкая - рано)
//    pibox        задней стенкой вниз               ~31.2 мм
//    pibox_lid    наружной гранью вниз              ~0.8 мм
//  На паузе кладёшь по 4 магнита, продолжаешь печать - слои их замуровывают.
// ============================================================

part = "all";
show_ghost = true;
$fn = 48;

/* ===== ТЕРМОВТУЛКИ (СВЕРЬ со своими - у брендов отличается!) ===== */
ins_m2_d  = 3.2; ins_m2_l  = 4.0;
ins_m25_d = 3.5; ins_m25_l = 5.0;
ins_m3_d  = 4.0; ins_m3_l  = 5.7;
ins_m5_d  = 6.4; ins_m5_l  = 9.5;
ins_slack = 1.5;
boss_wall = 2.0;

/* ===== общее ===== */
wall     = 2.5;
mag_d    = 5.0;  mag_h = 2.0;    // <<< магниты 5x2
mag_pd   = mag_d + 0.2;          // карман Ø5.2
mag_ceil = 0.4;                  // потолок над магнитом со стороны стыка
lid_gap  = 0.35;
cham     = 0.5;
lid_th   = mag_h + mag_ceil + 0.8;   // 3.2: магнит 2 + потолок 0.4 + внешняя корка 0.8

/* ===== компоненты ===== */
// Raspberry Pi 5 (официально)
pi_l = 85; pi_w = 56; pi_pcb = 1.6;
pi_hx = 58; pi_hy = 49;          // крепёж 58x49, M2.5
stand_h = 4.5;                   // стойки под платой (зазор под компоненты снизу)
cooler_h = 15;                   // Active Cooler над платой (~13.7 + запас) VERIFY
fan_intake = 8;                  // ОБЯЗАТЕЛЬНЫЙ зазор над турбинкой
// Camera Module 3 (официально 25 x 24 x 11.5)
cam_l = 25; cam_w = 24; cam_h = 11.5; cam_fit = 0.25;
cam_lens_d = 11; cam_lens_d2 = 14;   // окно-КОНУС, расширяется наружу (поле 66x41)
// ИК Vishay TSHA5203 5мм
ir_body_d = 5.05; ir_aper_d = 3.6; ir_spacing = 44;
// CH9329 USB-A донгл (VERIFY - померить свой)
ch_l = 34; ch_w = 15; ch_h = 9;

/* ===== ПОЗИЦИИ РАЗЪЁМОВ Pi 5 (с офиц. чертежа, центры от угла платы) =====
   Питание/HDMI - на КОРОТКОМ ребре 56 мм.  USB/Ethernet - на ДЛИННОМ 85 мм. */
p_pwr=[11.2,25.8,39.2];          // USB-C, HDMI0, HDMI1   вдоль 56 мм
p_lan=[10.2,29.1,47.0];          // Ethernet, USB, USB    вдоль 85 мм
// РЕАЛЬНЫЕ ГАБАРИТЫ РАЗЪЁМОВ [ширина вдоль ребра, высота над платой], клиренс ~0.8 включён.
// Проёмы снуг, по телу разъёма - как в заводском корпусе, не "дыры".
// Замер пользователя: USB-стек 15, Ethernet 14. USB-C/HDMI низкие (~3.5) -> проём НИЗКИЙ.
c_usbc=[9.5, 5.5];    // USB-C питание
c_hdmi=[8.5, 5.5];    // micro-HDMI (плаг тонкий; овермолд остаётся снаружи)
c_usb =[14.5, 16.0];  // USB-A двойной стек (15 + клиренс)
c_lan =[16.5, 15.0];  // RJ45 Ethernet (14 + клиренс)
c_sd  =[13.0, 3.5];   // microSD
port_ch=0.8;          // фаска-заход по наружной грани (профессиональный вид)
// ПОЗИЦИИ ПО ОФИЦ.ЧЕРТЕЖУ, плата повёрнута CAM-ребром вверх (90° CW):
//  НИЗ (-Z): Ethernet/USB/USB по X = plx0 + p_bot   (=drawing right-edge 10.2/29.1/47)
//  ЛЕВО(-X): USB-C/HDMI0/HDMI1 по Z = plz0 + p_left (=85 - drawing 11.2/25.8/39.2)
p_bot =[10.2, 29.1, 47.0];
p_left=[73.8, 59.2, 45.8];

/* ===== бар ===== */
bar_l=118; bar_d=16; bar_h=18;
boss_w=cam_l+2*wall+4; boss_h=cam_w+2*wall+1;     // прилив под камеру ~30 (BAR-01)

/* ===== тиски ===== */
// clamp_reach: раскрытие под монитор = reach - 2*jaw_t - толщина пятки(~4).
// Для монитора до 30 мм: 30 + 14 + 4 = 48.
clamp_w=56; clamp_reach=48; top_t=7; jaw_t=7;
rear_jaw_h=48; front_lip_h=7; hinge_r=7; hinge_w=10; fork_gap=hinge_w+0.8;

/* ===== Pi-БОКС: плата 85 ВЕРТИКАЛЬНО (Z), 56 по X, глубина Y =====
   +Z (верх) = CAM/DISP -> шлейф к бару + док к тискам
   -Z (низ)  = питание/HDMI + выход USB CH9329
   +X        = USB/Ethernet
   -X        = GPIO внутрь + отсек CH9329
   -Y = глухая задняя стенка (к монитору), +Y = горловина/крышка */
plate_X=pi_w; plate_Z=pi_l;              // плата: 56 по X, 85 по Z
// поля: СЛЕВА узко (стенка питания/HDMI), СПРАВА широко (GPIO + отсек CH9329)
m_left=2.5; m_right=14; m_bot=11; m_top=10;
box_ix=m_left+pi_w+m_right;              // X внутр
box_iz=m_bot+pi_l+m_top;                 // Z внутр
box_iy=stand_h+pi_pcb+cooler_h+fan_intake+2;  // Y внутр
box_x=box_ix+2*wall; box_z=box_iz+2*wall; box_y=box_iy+wall;
// базы
plx0=-box_ix/2+m_left;  plz0=wall+m_bot;      // угол платы (min X, min Z)
pcx=plx0+pi_w/2; pcz=plz0+pi_l/2;             // центр платы
pby=-box_y/2+wall+stand_h+pi_pcb;             // верх PCB (Y), от него растут разъёмы

// ============================================================
//  ХЕЛПЕРЫ
// ============================================================
// r автоматически ограничивается полутолщиной, иначе тонкие плиты "раздувает"
module rbox(l,w,h,r=3){ rr=min(r,l/2-0.01,w/2-0.01); hull() for(x=[rr-l/2,l/2-rr]) for(y=[rr-w/2,w/2-rr]) translate([x,y,0]) cylinder(r=rr,h=h); }
module insert_hole(d,l){ union(){ cylinder(d=d,h=l+ins_slack); translate([0,0,-0.01]) cylinder(d1=d+1.2,d2=d,h=0.9); } }
module boss(di,li,h){ difference(){ cylinder(d=di+2*boss_wall,h=h); translate([0,0,h-li-ins_slack]) insert_hole(di,li); } }
// магнитный карман: локальная система - деталь стыкуется гранью +Y, карман
// открыт в +Y, сверху потолок mag_ceil. Вызывать в координатах грани стыка.
module mag_hole_pY(faceY){ translate([0,faceY-mag_ceil,0]) rotate([90,0,0]) cylinder(d=mag_pd,h=mag_h); }
// Проём разъёма с фаской-заходом по наружной грани. Локально: отверстие вдоль +Z,
// наружная грань на z=thick. w вдоль X, h вдоль Y.
module port_slot(w,h,thick){
  translate([0,0,-0.1]) linear_extrude(thick+0.2) square([w,h],center=true);
  translate([0,0,thick-port_ch]) linear_extrude(port_ch+0.1,scale=[(w+2*port_ch)/w,(h+2*port_ch)/h]) square([w,h],center=true);
}
// проём в НИЖНЕЙ грани (-Z): выход наружу вниз
module port_bottom(cx,w,h){ translate([cx, pby+h/2-0.5, wall]) rotate([180,0,0]) port_slot(w,h,wall); }
// проём в БОКОВОЙ грани (+X): выход наружу вбок (w вдоль Z, h вдоль Y)
module port_sideX(cz,w,h){ translate([box_x/2-wall, pby+h/2-0.5, cz]) rotate([0,90,0]) port_slot(w,h,wall); }
// проём в ЛЕВОЙ грани (-X): выход наружу влево (w вдоль Z, h вдоль Y)
module port_left(cz,w,h){ translate([-box_x/2+wall, pby+h/2-0.5, cz]) rotate([0,-90,0]) port_slot(w,h,wall); }

// ============================================================
//  БАР КАМЕРЫ  (печать: окнами ВНИЗ; стык/магниты - грань +Y вверх)
// ============================================================
function mag_x(sx,k)=sx*(k?bar_l/2-10:boss_w/2+10);
module hinge_fork(){
  kw=8;
  difference(){
    union(){
      for(sx=[-1,1]) translate([sx*(fork_gap/2+kw/2),0,-hinge_r]) rotate([0,90,0]) cylinder(r=hinge_r,h=kw,center=true);
      for(sx=[-1,1]) translate([sx*(fork_gap/2+kw/2),0,-hinge_r/2+1]) cube([kw,hinge_r*1.5,hinge_r+2],center=true);
    }
    translate([-(fork_gap/2+kw+1),0,-hinge_r]) rotate([0,90,0]) cylinder(d=3.4,h=kw+2);
    translate([0,0,-hinge_r]) rotate([0,90,0]) cylinder(d=3.4,h=fork_gap+1,center=true);
    translate([fork_gap/2+kw+0.01,0,-hinge_r]) rotate([0,-90,0]) insert_hole(ins_m3_d,ins_m3_l);  // фиксация угла (HNG-01)
  }
}
module bar(){
  difference(){
    union(){
      difference(){
        union(){
          rbox(bar_l,bar_d,bar_h,3);
          rbox(boss_w,bar_d,boss_h,3);
          for(sx=[-1,1]) translate([sx*(boss_w/2+1),-bar_d/2+wall,wall]) cube([2,bar_d-2*wall,boss_h-2*wall]);      // баффлы (OPT-01)
          for(sx=[-1,1]) translate([sx*(bar_l/4),-bar_d/2+wall,wall]) cube([2,3.5,bar_h-2*wall]);                   // рёбра (BAR-05)
          for(sx=[-1,1]) translate([sx*(cam_l/2+5),-bar_d/2+wall,boss_h/2]) rotate([-90,0,0]) boss(ins_m25_d,ins_m25_l,8);  // планка камеры
        }
        translate([-(bar_l/2-wall),-(bar_d/2-wall),wall]) cube([bar_l-2*wall,bar_d,bar_h-2*wall]);      // полость крыльев
        translate([-(boss_w/2-wall),-(bar_d/2-wall),wall]) cube([boss_w-2*wall,bar_d,boss_h-2*wall]);   // полость прилива
        translate([-(cam_l+cam_fit)/2,-bar_d/2+wall,boss_h/2-(cam_w+cam_fit)/2]) cube([cam_l+cam_fit,cam_h+1.5,cam_w+cam_fit]); // карман камеры
        translate([0,-bar_d/2+wall+0.01,boss_h/2]) rotate([90,0,0]) cylinder(d1=cam_lens_d,d2=cam_lens_d2,h=wall+0.02);        // окно-конус
        for(sx=[-1,1]){
          translate([sx*ir_spacing/2,-bar_d/2+wall+0.01,bar_h/2]) rotate([-90,0,0]) cylinder(d=ir_body_d,h=7);
          translate([sx*ir_spacing/2,-bar_d/2+wall+0.02,bar_h/2]) rotate([90,0,0]) cylinder(d=ir_aper_d,h=wall+0.04);
          translate([sx*(ir_spacing/2)+4,-bar_d/2+wall,bar_h/2-5.5]) cube([10,4,3]);
        }
        translate([16,-bar_d/2+wall,-1]) cube([24,bar_d-2*wall,wall+3]);   // выход шлейфа вниз, сбоку от вилки (BAR-03)
      }
      for(sx=[-1,1]) for(k=[0,1]) translate([mag_x(sx,k),bar_d/2-2,bar_h/2]) cube([9,4,bar_h-2*wall],center=true);  // столбики магнитов
    }
    for(sx=[-1,1]) for(k=[0,1]) translate([mag_x(sx,k),0,bar_h/2]) mag_hole_pY(bar_d/2);   // магниты
  }
  hinge_fork();
}
// стыковая грань = +Y (y=0). Плита y[-wall..0], магнит сидит В ПЛИТЕ (2.4<2.5).
module bar_lid(){
  difference(){
    union(){
      translate([0,-lid_th/2,0]) rbox(bar_l,lid_th,bar_h,3);                          // плита lid_th, стык +Y на 0
      translate([0,0.9,0]) rbox(bar_l-2*wall-lid_gap,1.8,bar_h-2*wall-lid_gap,2);      // бортик +Y (в бар)
    }
    for(sx=[-1,1]) for(k=[0,1]) translate([mag_x(sx,k),0,bar_h/2]) mag_hole_pY(0);      // магнит: стык +Y, потолок 0.4
  }
}
module cam_clip(){ difference(){ rbox(cam_l+20,6,3,2); for(sx=[-1,1]) translate([sx*(cam_l/2+5),0,-1]) cylinder(d=2.9,h=6); } }  // отв. совпадают с бобышками бара

// ============================================================
//  ТИСКИ  (+ док Pi-бокса = противовес)
// ============================================================
module clamp(){
  R=clamp_reach; hy=-R/2+13;
  difference(){
    union(){
      rbox(clamp_w,R,top_t,4);
      translate([0,R/2-jaw_t/2,top_t-rear_jaw_h]) rbox(clamp_w,jaw_t,rear_jaw_h,3);        // задняя губка
      translate([0,-R/2+jaw_t/2,top_t-front_lip_h]) rbox(clamp_w,jaw_t,front_lip_h,3);      // передняя губа
      translate([0,hy,top_t+hinge_r-1]) rotate([0,90,0]) cylinder(r=hinge_r,h=hinge_w,center=true);   // кулак
      translate([0,hy,(top_t+hinge_r)/2]) cube([hinge_w,hinge_r*1.6,top_t+hinge_r],center=true);
      translate([0,R/2-jaw_t,top_t-rear_jaw_h*0.55]) rotate([-90,0,0]) cylinder(d=ins_m5_d+2*boss_wall+2,h=jaw_t+7);   // бобышка M5
      for(sx=[-1,1]) translate([sx*18,R/2-jaw_t,top_t-rear_jaw_h+12]) rotate([-90,0,0]) cylinder(d=ins_m3_d+2*boss_wall,h=jaw_t+4);  // бобышки дока
    }
    translate([0,hy,top_t+hinge_r-1]) rotate([0,90,0]) cylinder(d=3.3,h=clamp_w,center=true);      // ось шарнира
    translate([0,R/2+8,top_t-rear_jaw_h*0.55]) rotate([90,0,0]) cylinder(d=5.4,h=jaw_t+20);        // проход винта M5
    translate([0,R/2+7,top_t-rear_jaw_h*0.55]) rotate([90,0,0]) insert_hole(ins_m5_d,ins_m5_l);    // втулка M5 с торца
    translate([0,-R/2+jaw_t-0.7,top_t-front_lip_h/2-0.5]) cube([clamp_w-14,1.4,front_lip_h-3],center=true);  // ниша TPU
    // канал шлейфа - на НИЖНЕЙ грани перемычки (между тисками и монитором):
    // верх остаётся ЦЕЛЬНЫМ -> нет "ушей", кулак стоит на сплошной тумбе (CLP-06)
    translate([-13,-R/2-1,-0.1]) cube([26,R+2,2.5]);
    for(sx=[-1,1]) translate([sx*18,R/2+4,top_t-rear_jaw_h+12]) rotate([90,0,0]) insert_hole(ins_m3_d,ins_m3_l);  // втулки дока
  }
}
module pad(){ rbox(clamp_w-15,1.4,front_lip_h-4,1.5); }
module foot(){ difference(){ union(){ rbox(22,4,16,3); translate([0,3.4,8]) rotate([-90,0,0]) cylinder(d=9,h=3); } translate([0,3.6,8]) rotate([-90,0,0]) sphere(d=5.4); } }

// ============================================================
//  PI-БОКС
// ============================================================
// 4 магнита СИММЕТРИЧНО (x=±24), на верхней/нижней полке
mag_box=[[-24,box_z-wall-4],[24,box_z-wall-4],[-24,wall+4],[24,wall+4]];
module pibox(){
  difference(){
    union(){
      difference(){
        rbox(box_x,box_y,box_z,4);
        translate([0,(wall+2)/2,wall]) rbox(box_ix,box_y-wall+2,box_iz,3);   // полость (глухая задняя стенка)
      }
      // ledge-полки под магниты сверху/снизу (в полях, плату не задевают)
      translate([0,box_y/2-2.2,box_z-wall-4]) cube([box_ix-8,4.4,8],center=true);
      translate([0,box_y/2-2.2,wall+4]) cube([box_ix-8,4.4,8],center=true);
      // 4 УГЛОВЫЕ ОПОРЫ: плата ЛЕЖИТ на них (solder-side), монтажные отверстия
      // остаются свободными под пины Active Cooler. Опоры inboard от углов - на ровный край PCB.
      for(sx=[0,1]) for(sz=[0,1]) translate([plx0+[5,plate_X-5][sx],-box_y/2+wall,plz0+[5,plate_Z-5][sz]]) rotate([-90,0,0]) cylinder(d=6,h=stand_h);
      // рёбра-дистанционники к монитору (снаружи задней стенки)
      for(sx=[-1,1]) translate([sx*(box_x/2-12),-box_y/2-7,box_z*0.3]) cube([6,7,box_z*0.4]);
      // док-уши вверх к тискам
      for(sx=[-1,1]) translate([sx*16-5,-box_y/2,box_z-2]) cube([10,box_y*0.55,14]);
      // отсек CH9329 СПРАВА (сторона GPIO), USB вниз (BOX-07)
      translate([box_ix/2-ch_h/2-1,-box_y/2+wall+ch_w/2+1,wall+ch_l/2+1])
        difference(){ cube([ch_h+3,ch_w+3,ch_l+3],center=true); cube([ch_h+0.4,ch_w+0.4,ch_l+0.4],center=true); translate([0,3,0]) cube([ch_h+5,ch_w+5,ch_l-12],center=true); }
      // НИЖНИЕ ПОЛКИ с передним бортиком: нижняя кромка платы ложится сюда и
      // ЗАХВАТЫВАЕТСЯ по Y (несут вес + не дают выпасть вперёд). Прижим платы -
      // внутри корпуса, снаружи ничего не видно (крышка остаётся чистой).
      for(x=[pcx-20,pcx+20]){
        translate([x-4,-box_y/2+wall,plz0-2]) cube([8,stand_h+pi_pcb,2]);                 // полка
        translate([x-4,-box_y/2+wall+stand_h+pi_pcb-0.5,plz0-2]) cube([8,1.5,5]);          // бортик спереди
      }
    }
    // === ПОРТЫ ПО ОФИЦИАЛЬНОМУ ЧЕРТЕЖУ (плата CAM-ребром вверх) ===
    // НИЗ (-Z): Ethernet + USB + USB (снуг, с фаской)
    port_bottom(plx0+p_bot[0], c_lan[0], c_lan[1]);         // Ethernet
    port_bottom(plx0+p_bot[1], c_usb[0], c_usb[1]);         // USB-стек
    port_bottom(plx0+p_bot[2], c_usb[0], c_usb[1]);         // USB-стек
    // ЛЕВО (-X): USB-C питание + 2x micro-HDMI
    port_left(plz0+p_left[0], c_usbc[0], c_usbc[1]);        // USB-C
    port_left(plz0+p_left[1], c_hdmi[0], c_hdmi[1]);        // HDMI0
    port_left(plz0+p_left[2], c_hdmi[0], c_hdmi[1]);        // HDMI1
    // microSD (VERIFY по факту): левая грань у USB-C, на solder-side (-Y)
    translate([-box_x/2+wall,-box_y/2+wall+2.5,plz0+80]) rotate([0,-90,0]) port_slot(c_sd[0],c_sd[1],wall);
    // выход USB от CH9329 вниз, справа
    translate([box_ix/2-8,-box_y/2+wall+ch_w/2,wall]) rotate([180,0,0]) port_slot(12,6,wall);
    // шлейф камеры вверх (+Z) к бару + провода ИК рядом
    translate([pcx-12,pby-1.5,box_z-wall-1]) cube([24,6,wall+3]);
    translate([pcx+15,-box_y/2+wall+1,box_z-wall-1]) cube([5,6,wall+3]);
    // --- ВЕНТИЛЯЦИЯ: СКВОЗНЫЕ щели (центр куба на стенке -> прорезает насквозь).
    //     Симметричные группы: верх = выброс, низ = приток.
    vcy = -box_y/2+wall+4+(box_iy-8)/2;
    for(sx=[-1,1]) for(i=[0:3]) translate([sx*box_x/2, vcy, box_z-15-i*6]) cube([wall*3,box_iy-8,2.4],center=true);  // выброс
    for(sx=[-1,1]) for(i=[0:3]) translate([sx*box_x/2, vcy, 13+i*6]) cube([wall*3,box_iy-8,2.4],center=true);        // приток
    // (индикатор/кнопку с ЗАДНЕЙ стенки убрал - она прижата к монитору, толку ноль.
    //  Если нужен статус-LED - его место на баре, лицом к пользователю; добавлю по запросу.)
    // --- магниты (интегрированы в ledge, не выступают)
    for(m=mag_box) translate([m[0],0,m[1]]) mag_hole_pY(box_y/2);
    // --- док: проходные M3 в ушах
    for(sx=[-1,1]) translate([sx*16,-box_y/2-1,box_z+5]) rotate([-90,0,0]) cylinder(d=3.4,h=box_y);
  }
}
// Крышка: ЧИСТАЯ и СИММЕТРИЧНАЯ. Решётка строго по центру, без "ножек"
// (прижим платы уведён в корпус). Магниты симметрично.
module pibox_lid(){
  gN=9; gsp=4.6; gh=42;                        // симметричная центральная решётка
  difference(){
    union(){
      translate([0,box_y/2+lid_th/2,0]) rbox(box_x,lid_th,box_z,4);              // плита
      translate([0,box_y/2-0.9,0]) rbox(box_ix-lid_gap,1.8,box_iz-lid_gap,3);    // бортик внутрь
      translate([0,box_y/2-0.9,wall+2]) cube([box_ix-12,5,5],center=true);       // зацеп снизу
    }
    // вентиляция кулера - симметричная решётка по ЦЕНТРУ крышки
    for(i=[0:gN-1]) translate([(i-(gN-1)/2)*gsp, box_y/2+wall/2, box_z/2]) cube([2.2,wall+6,gh],center=true);
    // ответные магниты (потолок 0.4 со стороны стыка, впаиваются на паузе)
    for(m=mag_box) translate([m[0],box_y/2+0.4,m[1]]) rotate([-90,0,0]) cylinder(d=mag_pd,h=mag_h);
  }
}

// ============================================================
//  ПРИЗРАКИ
// ============================================================
module ghost_cam(){ color([0.2,0.4,0.8,0.5]) cube([cam_l,cam_h,cam_w],center=true); }
module ghost_pi(){ color([0.2,0.6,0.2,0.35]) translate([pcx,pby+pi_pcb/2,pcz]) cube([pi_w,pi_pcb,pi_l],center=true); }

// ============================================================
//  ВЫВОД
// ============================================================
if      (part=="bar")       bar();
else if (part=="bar_lid")   bar_lid();
else if (part=="cam_clip")  cam_clip();
else if (part=="clamp")     clamp();
else if (part=="pibox")     pibox();
else if (part=="pibox_lid") pibox_lid();
else if (part=="pad")       pad();
else if (part=="foot")      foot();
else {
  if(show_ghost) color([0.12,0.12,0.13,0.6]) translate([-90,-clamp_reach/2+jaw_t,-190]) cube([180,10,190]);
  clamp();
  hy=-clamp_reach/2+13;
  translate([0,hy,top_t+hinge_r-1]) rotate([-38,0,0]) translate([0,0,hinge_r]){ bar(); translate([0,bar_d/2+10,0]) rotate([180,0,0]) bar_lid(); if(show_ghost) translate([0,-bar_d/2+wall+cam_h/2,boss_h/2]) ghost_cam(); }
  translate([0,clamp_reach/2+jaw_t+box_y/2+2,top_t-rear_jaw_h-box_z+14]){ pibox(); if(show_ghost) ghost_pi(); translate([0,box_y+30,0]) pibox_lid(); }
}
