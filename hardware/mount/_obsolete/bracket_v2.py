import cadquery as cq
import math

# ============================================================
# PARAMETERS - Pi-Gaze tail bracket (Phase 2: features)
#   Hook over monitor top edge + M3 set-screw grip; spine down
#   the back; tray carries the Pi 5 case (cover-side to spine,
#   vented floor facing OUT); ribbon channel up the spine and
#   over the hook (enclosed later by the snap-on duct cover).
# ============================================================
# --- Monitor top-edge interface ---
bezel_t     = 15.0   # mm top-edge thickness (user: ~13-16, use 15)
throat_fit  = 1.0    # mm extra throat clearance so it drops on easily
hook_wall   = 4.0    # mm hook material thickness
front_lip   = 18.0   # mm front lip drop (also houses the set screw)

# --- M3 set screw in the front lip (presses the monitor front) ---
ss_d        = 3.3    # mm M3 clearance / thread-forming hole
ss_nut_af   = 5.5    # mm M3 nut across-flats
ss_nut_th   = 2.6    # mm nut pocket depth
ss_boss     = 6.0    # mm extra material outside the lip for the nut

# --- Spine (down the monitor back) ---
spine_w     = 66.0   # mm width along the monitor edge
spine_h     = 116.0  # mm drop down the back
spine_t     = 5.0    # mm spine thickness (holds the ribbon channel)

# --- Case tray (case cover-side against spine, long axis vertical) ---
case_len    = 96.6   # mm case long dim (vertical)
case_wid    = 65.0   # mm case short dim (along Y)
case_th     = 29.5   # mm case thickness (stands off in -X)
fit         = 0.4    # mm clearance
case_top_z  = -6.0   # mm case top (Pi power/HDMI/CSI end) sits here
shelf_th    = 3.0    # mm bottom shelf
lip_h       = 14.0   # mm locating side lips (short: don't cover side ports)
lip_t       = 3.0    # mm side lip thickness
strap_w     = 12.0   # mm strap slot width
strap_t     = 3.5    # mm strap slot height (for velcro / zip tie)

# --- Ribbon channel (open groove; enclosed by duct cover part) ---
chan_w      = 15.0   # mm channel width (22-pin FPC ~12mm + margin)
chan_d      = 3.5    # mm channel depth into the surface

eps = 0.02
mx = bezel_t + throat_fit                      # front face of throat (monitor front)

# ============================================================
# MODEL  (+X = monitor FRONT, -X = back, +Z up, top plane Z=0)
# ============================================================
# --- Hook: bridge over the top + front lip (with set-screw boss) ---
bridge = (cq.Workplane("XY")
    .box(spine_t + mx + hook_wall, spine_w, hook_wall, centered=(False, True, False))
    .translate((-spine_t, 0, 0)))
lip = (cq.Workplane("XY")
    .box(hook_wall, spine_w, front_lip + hook_wall, centered=(False, True, False))
    .translate((mx, 0, -front_lip)))
boss = (cq.Workplane("XY")
    .box(ss_boss + eps, ss_nut_af + 5, ss_nut_af + 5, centered=(False, True, True))
    .translate((mx + hook_wall - eps, 0, -front_lip / 2)))

# --- Spine down the back ---
spine = (cq.Workplane("XY")
    .box(spine_t, spine_w, spine_h + hook_wall, centered=(False, True, False))
    .translate((-spine_t, 0, -spine_h)))

# --- Tray: bottom shelf + two short locating lips ---
case_x1 = -spine_t
case_x0 = case_x1 - (case_th + fit)
case_bot = case_top_z - case_len
shelf = (cq.Workplane("XY")
    .box(case_th + fit + lip_t, case_wid + 2 * fit + 2 * lip_t, shelf_th, centered=(False, True, False))
    .translate((case_x0 - lip_t, 0, case_bot - shelf_th)))
lips = cq.Workplane("XY")
for sy in (-1, 1):
    y0 = sy * (case_wid / 2 + fit)
    lips = lips.union(cq.Workplane("XY")
        .box(case_th + fit, lip_t, lip_h, centered=(False, False, False))
        .translate((case_x0, y0 if sy > 0 else y0 - lip_t, case_bot)))

body = bridge.union(lip).union(boss).union(spine).union(shelf).union(lips)

# ============================================================
# CUTS
# ============================================================
# set-screw: through-hole along X + hex nut pocket open to the outer face
ss_x = mx + hook_wall + ss_boss                 # outer face of the boss
nut_dia = ss_nut_af / math.cos(math.radians(30))  # across-corners
setscrew = (cq.Workplane("YZ").workplane(offset=ss_x + eps)
    .center(0, -front_lip / 2)                  # YZ plane -> (Y, Z); X set by offset
    .circle(ss_d / 2).extrude(-(ss_x + eps - mx)))   # bore inward to the monitor front
nut = (cq.Workplane("YZ").workplane(offset=ss_x + eps)
    .center(0, -front_lip / 2)
    .polygon(6, nut_dia).extrude(-ss_nut_th))
body = body.cut(setscrew).cut(nut)

# ribbon channel: groove up the spine (-X face) from case top, over the bridge, down front lip
chan_up = (cq.Workplane("XY")
    .box(chan_d + eps, chan_w, (0 - case_top_z) + 30, centered=(False, True, False))
    .translate((-spine_t - eps, 0, case_top_z)))          # into -X face of spine
chan_top = (cq.Workplane("XY")
    .box(spine_t + mx + hook_wall, chan_w, chan_d + eps, centered=(False, True, False))
    .translate((-spine_t, 0, hook_wall - chan_d)))        # groove along top of bridge
chan_front = (cq.Workplane("XY")
    .box(chan_d + eps, chan_w, front_lip, centered=(False, True, False))
    .translate((mx + hook_wall - chan_d, 0, -front_lip)))  # groove down the front lip face
body = body.cut(chan_up).cut(chan_top).cut(chan_front)

# strap slots: two horizontal slots through the spine, above the shelf
for zc in (case_bot + 22, case_bot + 60):
    slot = (cq.Workplane("XY")
        .box(spine_t + 2 * eps, strap_w, strap_t, centered=(False, True, True))
        .translate((-spine_t - eps, 0, zc)))
    body = body.cut(slot)

result = body
cq.exporters.export(result, "bracket_v2.stl", tolerance=0.01, angularTolerance=0.1)
print("Exported bracket_v2.stl")
