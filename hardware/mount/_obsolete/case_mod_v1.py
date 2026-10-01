"""
Pi-Gaze: modify the READY-MADE case's OUTER SHELL (no adapter).
Features authored in CadQuery, fused into the case mesh with the Manifold engine.
Only the outer shell is changed; ports and the internal cavity are untouched.

Case frame (measured): X[-48,48.6] long, Y[-32.5,32.5] wide, Z[-2.5,27] thick.
Hangs with +X end UP and the COVER side (+Z) toward the monitor; the Pi HDMI/CSI
edge is at +X, so the camera ribbon exits near the +X / +Z corner and runs up to
the bar. Dock = 2x M3 @ +-22 (matches the existing clamp dock).
"""
import cadquery as cq
import trimesh, os

CASE   = r"C:\Users\bronl\Downloads\RPi 5 case.stl"
OUT    = r"C:\Users\bronl\Desktop\pi-gaze\hardware\mount\case_mod_v1.stl"
TMP    = r"C:\Users\bronl\AppData\Local\Temp\claude\C--Users-bronl-Desktop-teleprompt\b4562cd9-b019-48d2-82b2-9d0e523f1b31\scratchpad"

# ---------------- parameters ----------------
top_x   = 48.6     # case +X end
cov_z   = 27.0     # cover outer plane
# ribbon
csi_x   = 30.0     # ribbon exit centre (X) - near HDMI/CSI edge (refine vs Pi)
slot_w  = 16.0     # ribbon slot width (Y)
slot_l  = 6.0      # ribbon slot length (X)
duct_w  = 18.0     # duct outer width (Y)
duct_h  = 5.0      # duct height above cover (Z)
duct_wall = 1.6
# dock mount (2x M3 @ +-22) on the +X-end / cover corner
dock_pitch = 44.0
boss_d  = 8.0
boss_h  = 7.0      # rises above cover (+Z)
m3      = 3.3
eps = 0.02

def cq2tm(solid, name):
    p = os.path.join(TMP, name)
    cq.exporters.export(solid, p, tolerance=0.01, angularTolerance=0.1)
    return trimesh.load(p, force='mesh')

# ---------------- ADD features (union) ----------------
add = cq.Workplane("XY")
# ribbon duct: a covered channel on the cover (+Z), from the slot out over the +X end
duct = (cq.Workplane("XY")
        .box(top_x + 8 - csi_x + 6, duct_w, duct_h, centered=(False, True, False))
        .translate((csi_x - 3, 0, cov_z - eps)))
duct_cav = (cq.Workplane("XY")
        .box(top_x + 8 - csi_x + 6, duct_w - 2*duct_wall, duct_h, centered=(False, True, False))
        .translate((csi_x - 3, 0, cov_z + duct_wall)))
duct = duct.cut(duct_cav)
# two dock bosses at the +X/cover corner
bosses = cq.Workplane("XY")
for sy in (-1, 1):
    bosses = bosses.union(
        cq.Workplane("XY").cylinder(boss_h, boss_d/2, centered=(True, True, False))
        .translate((top_x - 4, sy*dock_pitch/2, cov_z - eps)))
add = duct.union(bosses)

# ---------------- CUT features (difference) ----------------
cuts = cq.Workplane("XY")
# ribbon exit slot through the cover
slot = (cq.Workplane("XY")
        .box(slot_l, slot_w, 12, centered=(True, True, True))
        .translate((csi_x, 0, cov_z - 4)))
# M3 holes through the bosses (along Z, into the dock)
holes = cq.Workplane("XY")
for sy in (-1, 1):
    holes = holes.union(cq.Workplane("XY").cylinder(boss_h + 8, m3/2, centered=(True, True, False))
        .translate((top_x - 4, sy*dock_pitch/2, cov_z - 4)))
cuts = slot.union(holes)

# ---------------- boolean with the case mesh (Manifold) ----------------
case = trimesh.load(CASE, force='mesh'); case.merge_vertices()
add_m = cq2tm(add, "add.stl")
cut_m = cq2tm(cuts, "cuts.stl")
res = trimesh.boolean.union([case, add_m])
res = trimesh.boolean.difference([res, cut_m])
res.export(OUT)
print("watertight=%s faces=%d vol=%.1fcm3 -> %s" % (res.is_watertight, len(res.faces), res.volume/1000, OUT))
