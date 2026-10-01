"""
Pi-Gaze: modify the READY-MADE case OUTER SHELL to hang on the existing clamp
dock and route the camera ribbon. Features authored in CadQuery, fused into the
case mesh with the Manifold engine. Ports & internal cavity untouched.

Case frame (measured): X[-48,48.6] long, Y[-32.5,32.5] wide, Z[-2.5,27] thick.
Hang orientation: +X end UP (Pi HDMI/CSI end), FLOOR (-Z) toward the monitor
(dock holds it ~45mm off, so floor vents still breathe). Dock mount on the solid
floor; ribbon exits the +X end wall near the top and runs up to the bar.
"""
import cadquery as cq
import trimesh, os

CASE = r"C:\Users\bronl\Downloads\RPi 5 case.stl"
OUT  = r"C:\Users\bronl\Desktop\pi-gaze\hardware\mount\case_mod_v2.stl"
TMP  = r"C:\Users\bronl\AppData\Local\Temp\claude\C--Users-bronl-Desktop-teleprompt\b4562cd9-b019-48d2-82b2-9d0e523f1b31\scratchpad"

# --------- parameters ---------
floor_z   = -2.5      # floor outer plane
xend      = 48.6      # +X end wall (outer)
dock_pitch = 44.0     # 2x M3 @ +-22 (matches clamp dock)
boss_x    = 36.0      # dock bosses X (clear of vents & corner screws)
boss_d    = 10.0
boss_h    = 7.0       # extends below floor (-Z) toward the dock
m3        = 3.3
# ribbon
slot_z0, slot_z1 = 20.5, 25.5   # ribbon slot band on the +X wall (top strip, above ports)
slot_w    = 16.0                # ribbon width (Y)
duct_len  = 16.0                # duct extension in +X (up, toward bar)
duct_h    = 7.0                 # trough depth
duct_wall = 1.6
eps = 0.03

def cq2tm(solid, name):
    p = os.path.join(TMP, name)
    cq.exporters.export(solid, p, tolerance=0.01, angularTolerance=0.1)
    m = trimesh.load(p, force='mesh'); m.merge_vertices(); return m

# ---------------- ADD (union) ----------------
# 2 dock bosses under the floor
add = cq.Workplane("XY")
for sy in (-1, 1):
    add = add.union(cq.Workplane("XY").cylinder(boss_h + eps, boss_d/2, centered=(True, True, False))
        .translate((boss_x, sy*dock_pitch/2, floor_z - boss_h)))
# ribbon duct: covered trough growing in +X from the top wall, guiding ribbon up
zc = (slot_z0 + slot_z1)/2
duct_out = (cq.Workplane("XY").box(duct_len, slot_w + 2*duct_wall, (slot_z1-slot_z0) + 2*duct_wall,
            centered=(False, True, True)).translate((xend - 2, 0, zc)))
duct_in  = (cq.Workplane("XY").box(duct_len + 4, slot_w, (slot_z1-slot_z0),
            centered=(False, True, True)).translate((xend - 3, 0, zc)))
add = add.union(duct_out.cut(duct_in))

# ---------------- CUT (difference) ----------------
cuts = cq.Workplane("XY")
# M3 through the bosses (blind into the floor frame; screw to dock)
for sy in (-1, 1):
    cuts = cuts.union(cq.Workplane("XY").cylinder(boss_h + 4, m3/2, centered=(True, True, False))
        .translate((boss_x, sy*dock_pitch/2, floor_z - boss_h - eps)))
# ribbon exit slot through the +X wall (top strip)
cuts = cuts.union(cq.Workplane("XY").box(12, slot_w, slot_z1 - slot_z0, centered=(True, True, True))
        .translate((xend, 0, zc)))

# ---------------- boolean with case mesh ----------------
case = trimesh.load(CASE, force='mesh'); case.merge_vertices()
res = trimesh.boolean.union([case, cq2tm(add, "add2.stl")])
res = trimesh.boolean.difference([res, cq2tm(cuts, "cuts2.stl")])
res.merge_vertices(); res.export(OUT)
print("watertight=%s faces=%d vol=%.1fcm3 bbox=%s" %
      (res.is_watertight, len(res.faces), res.volume/1000,
       [round(float(v),1) for v in res.extents]))
