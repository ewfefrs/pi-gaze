"""
Pi-Gaze case shell mod v3 - mounts on the EXISTING clamp dock.
Floor gets 2 flat M3 clearance holes @ +-22 (screws from inside into the dock
nut-traps, like the old box); ribbon exits the +X wall top into a duct. Features
authored in CadQuery, fused into the case mesh (Manifold). Ports/cavity untouched.
Frame: X[-48,48.6] long, Y[-32.5,32.5] wide, Z[-2.5,27] thick.
Hang: +X UP, floor(-Z) toward monitor. Holes at X=44 (past Pi board edge +42.6).
"""
import cadquery as cq
import trimesh, os

CASE = r"C:\Users\bronl\Downloads\RPi 5 case.stl"
OUT  = r"C:\Users\bronl\Desktop\pi-gaze\hardware\mount\case_mod_v3.stl"
TMP  = r"C:\Users\bronl\AppData\Local\Temp\claude\C--Users-bronl-Desktop-teleprompt\b4562cd9-b019-48d2-82b2-9d0e523f1b31\scratchpad"

floor_out = -2.5
xend      = 48.6
dock_pitch = 44.0
hole_x    = 44.0     # past Pi board HDMI edge (+42.6) -> screw head clears the board
m3c       = 3.6      # M3 clearance
# ribbon
slot_z0, slot_z1 = 20.0, 25.5
slot_w    = 16.0
duct_len  = 16.0
duct_wall = 1.6
eps = 0.03

def cq2tm(solid, name):
    p = os.path.join(TMP, name); cq.exporters.export(solid, p, tolerance=0.01, angularTolerance=0.1)
    m = trimesh.load(p, force='mesh'); m.merge_vertices(); return m

# ---- ADD: ribbon duct (covered trough up the +X end) ----
zc = (slot_z0 + slot_z1)/2
duct_out = (cq.Workplane("XY").box(duct_len, slot_w + 2*duct_wall, (slot_z1-slot_z0)+2*duct_wall,
            centered=(False, True, True)).translate((xend-2, 0, zc)))
duct_in  = (cq.Workplane("XY").box(duct_len+6, slot_w, (slot_z1-slot_z0),
            centered=(False, True, True)).translate((xend-3, 0, zc)))
add = duct_out.cut(duct_in)

# ---- CUT: 2 floor M3 clearance holes + ribbon slot ----
cuts = cq.Workplane("XY")
for sy in (-1, 1):
    cuts = cuts.union(cq.Workplane("XY").cylinder(12, m3c/2, centered=(True, True, True))
        .translate((hole_x, sy*dock_pitch/2, floor_out+2)))
cuts = cuts.union(cq.Workplane("XY").box(12, slot_w, slot_z1-slot_z0, centered=(True, True, True))
        .translate((xend, 0, zc)))

case = trimesh.load(CASE, force='mesh'); case.merge_vertices()
res = trimesh.boolean.union([case, cq2tm(add, "add3.stl")])
res = trimesh.boolean.difference([res, cq2tm(cuts, "cuts3.stl")])
res.merge_vertices(); res.update_faces(res.nondegenerate_faces()); res.export(OUT)
print("watertight=%s faces=%d vol=%.1fcm3 bbox=%s"
      % (res.is_watertight, len(res.faces), res.volume/1000, [round(float(v),1) for v in res.extents]))
