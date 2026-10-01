"""
Pi-Gaze — modification of the READY-MADE RPi 5 case (outer shell only).

Features are authored in CadQuery and fused into the case/cover meshes with the
Manifold engine (the originals are meshes, not B-reps). Ports, standoffs and
the internal cavity are NOT touched.

Case frame (measured from the STL): X[-48,48.6] long, Y[-32.5,32.5] wide,
Z[-2.5,27] (floor outer -2.5, floor top 0, cover 24..27). End wall -X:
outer -47, inner -44.5 (a power-button flexure lever pokes to -48
at Y~-10, Z~4 — keep clear). Pi 5 sits on standoffs at Z 2.90 (PCB top 4.50);
its CSI connectors are on the -Y (HDMI) edge at X 5..14.

HANG: the case hangs straight under the clamp's rear jaw, SD end (-X) UP,
cover toward the monitor, USB/Ethernet down. Case -> clamp transform HANG_T.
The camera ribbon runs under the cover along -X at Z~20 and leaves through a
slot in the top end wall — that line continues straight up the tunnel inside
the rear jaw (see enclosure/src/clamp.scad RIB_*).
"""
import cadquery as cq
import trimesh, os, json
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
CASE_IN  = r"C:\Users\bronl\Downloads\RPi 5 case.stl"
COVER_IN = r"C:\Users\bronl\Downloads\RPi 5 cover blank.stl"
CASE_OUT  = os.path.join(HERE, "case_pigaze.stl")
COVER_OUT = os.path.join(HERE, "cover_pigaze.stl")
TMP = os.path.join(HERE, "_tmp"); os.makedirs(TMP, exist_ok=True)

# ---------------------------------------------------------------- hang transform
# clampX = caseY + 3.5 ; clampY = -caseZ + 40.5 ; clampZ = -caseX - 94
HANG_T = [[0, 1, 0, 3.5], [0, 0, -1, 40.5], [-1, 0, 0, -94.0], [0, 0, 0, 1]]
# real Pi 5 STL (raw) -> case frame
PI_T   = [[1, 0, 0, 0.1], [0, 0, -1, 0.0], [0, 1, 0, 2.88], [0, 0, 0, 1]]

# ---------------------------------------------------------------- parameters
END_OUT, END_IN = -47.0, -44.5    # true faces of the -X end wall (bbox -48 is the power-button lever)
ATT = END_OUT + 1.0                  # added features sink 1 mm into the 2.5 mm wall (fused, not touching)
BED = -2.45                          # features start 0.05 above the bed plane (no coplanar faces)
# ribbon slot in the top end wall (above the fan top 15.8, below cover 24.0)
RIB_Y0, RIB_Y1 = -28.5, -11.5        # 17 mm: cable 16 mm (22->15 pin Pi 5 cable)
RIB_Z0, RIB_Z1 = 17.5, 22.5          # 5 mm: ribbon + 2 IR wires
# collar: closes the 5 mm gap between the case top and the jaw's bottom face
COL_X_TOP = -52.9                    # -> clamp Z -41.1 (0.1 below the jaw)
COL_Y0, COL_Y1, COL_Z0, COL_Z1 = -30.0, -10.0, 16.0, 24.0
# dock tab (rises beside the jaw's outer face), 2x M3 -> nuts in the jaw
TAB_X_TOP = -62.0
TAB_Y0, TAB_Y1 = 6.5, 22.5
TAB_Z0, TAB_Z1 = 10.0, 16.4          # Z1 -> clamp Y 24.1 (0.1 off the jaw face); Z0 -> Y 30.5 (knob face 31)
TAB_HOLES = [(-58.0, 11.0), (-58.0, 18.0)]   # -> clamp X 14.5 / 21.5, Z -36
M3, M3_HEAD, CB_TOP = 3.4, 6.2, 12.8          # counterbore: head bears at Z 12.8
WELL_D = 6.6                                  # hex-key well through the gusset (7.0 made the two wells tangent -> non-manifold)
# CH9329 cradle on the +Y (GPIO) wall
CH_X0, CH_X1 = -30.0, 4.0            # module 34 long (verify your module!)
CH_Z0, CH_Z1 = 4.5, 19.5             # 15 wide
CH_Y0, CH_Y1 = 32.6, 41.8            # 9 thick + 0.2
WIRE_SLOT = (-37.0, -32.5, 15.0, 21.0)   # X0,X1,Z0,Z1 through the +Y wall
# cover fan grille (Active Cooler blower: X -37..-14, Y -22..20)
GRILLE_X = [-33.5, -29.0, -24.5, -20.0]; GRILLE_W = 3.0; GRILLE_Y = (-9.0, 19.0)

def box(x0, x1, y0, y1, z0, z1):
    return cq.Workplane("XY").box(x1-x0, y1-y0, z1-z0, centered=False).translate((x0, y0, z0))

def prism_xz(pts, y0, y1):
    """Triangle/polygon in the XZ plane extruded along Y from y0 to y1."""
    return (cq.Workplane("XZ").polyline(pts).close().extrude(-(y1-y0)).translate((0, y0, 0)))

def zcyl(x, y, d, z0, z1):
    return cq.Workplane("XY").circle(d/2).extrude(z1-z0).translate((x, y, z0))

def finalize(m, path):
    """Save, reload as a slicer would (float32 STL), repair any collapsed sliver, re-save."""
    m.export(path)
    r = trimesh.load(path, force='mesh')
    for _ in range(3):
        if r.is_watertight: break
        # collapse only the sub-0.05 mm edges that are shared by !=2 faces (float32 slivers)
        e = r.edges_sorted; u, c = np.unique(e, axis=0, return_counts=True)
        bad = [ed for ed in u[c != 2] if np.linalg.norm(r.vertices[ed[0]] - r.vertices[ed[1]]) < 0.05]
        if not bad: break
        V = r.vertices.copy(); F = r.faces.copy()
        for a, b in bad:
            V[b] = V[a]; F[F == b] = a
        r = trimesh.Trimesh(V, F, process=False)
        r.update_faces(r.nondegenerate_faces()); r.update_faces(r.unique_faces())
        r.remove_unreferenced_vertices()
        r.export(path); r = trimesh.load(path, force='mesh')
    return r

def to_mesh(shape, name):
    p = os.path.join(TMP, name)
    cq.exporters.export(shape, p, tolerance=0.01, angularTolerance=0.1)
    m = trimesh.load(p, force='mesh'); m.merge_vertices(); return m

# ---------------------------------------------------------------- CASE: additions
add = box(COL_X_TOP, ATT, COL_Y0, COL_Y1, COL_Z0, COL_Z1)               # collar
add = add.union(prism_xz([(ATT, COL_Z0), (COL_X_TOP, COL_Z0),          # its 45deg support
                          (ATT, COL_Z0-(ATT-COL_X_TOP))], COL_Y0, COL_Y1))
add = add.union(box(TAB_X_TOP, ATT, TAB_Y0, TAB_Y1, TAB_Z0, TAB_Z1))  # dock tab
add = add.union(prism_xz([(ATT, TAB_Z0), (TAB_X_TOP, TAB_Z0),          # gusset down to the bed
                          (ATT, BED)], TAB_Y0, TAB_Y1))
# CH9329 cradle: bottom block + end walls + outer lips + snap hooks (all rise from the bed plane)
add = add.union(box(CH_X0-2, CH_X1+2, 32.2, 43.6, BED, CH_Z0))
for x0, x1 in ((CH_X0-2, CH_X0), (CH_X1, CH_X1+2)):
    add = add.union(box(x0, x1, 32.2, 43.6, BED, CH_Z1+1.5))
for x0, x1 in ((CH_X0, CH_X0+1.5), (CH_X1-1.5, CH_X1)):
    add = add.union(box(x0, x1, CH_Y1+0.1, 43.6, CH_Z0, CH_Z1+1.5))           # outer lips
    add = add.union(box(x0, x1, 32.2, 43.6, CH_Z1+0.2, CH_Z1+1.5))            # snap hooks over the top edge

# ---------------------------------------------------------------- CASE: cuts
cut = box(COL_X_TOP-1, END_IN+0.3, RIB_Y0, RIB_Y1, RIB_Z0, RIB_Z1)            # slot through wall + collar
for (x, y) in TAB_HOLES:
    cut = cut.union(zcyl(x, y, M3, CB_TOP-0.1, TAB_Z1+1))                       # shank
    cut = cut.union(zcyl(x, y, M3_HEAD, TAB_Z0-0.1, CB_TOP))                    # counterbore
    cut = cut.union(zcyl(x, y, WELL_D, -3.0, TAB_Z0+0.05))                      # key well through gusset
# 3 mm chamfer on the tab's top corner nearest the thumbscrew knob (finger room)
cut = cut.union(cq.Workplane("XY").polyline([(TAB_X_TOP-0.1, TAB_Y0-0.1), (TAB_X_TOP+3.1, TAB_Y0-0.1),
                                             (TAB_X_TOP-0.1, TAB_Y0+3.1)]).close()
                .extrude(TAB_Z1-BED+0.4).translate((0, 0, BED-0.2)))          # through tab AND gusset
cut = cut.union(box(CH_X1-0.5, CH_X1+2.5, 32.6, 44.0, 8.0, 17.5))             # USB opening in the end wall
x0, x1, z0, z1 = WIRE_SLOT
cut = cut.union(box(x0, x1, 29.9, 33.2, z0, z1))                               # GPIO wires -> CH9329

case = trimesh.load(CASE_IN, force='mesh'); case.merge_vertices()
res = trimesh.boolean.union([case, to_mesh(add, "case_add.stl")])
res = trimesh.boolean.difference([res, to_mesh(cut, "case_cut.stl")])
res = finalize(res, CASE_OUT)

# ---------------------------------------------------------------- COVER: fan grille
gcut = None
for x in GRILLE_X:
    s = box(x-GRILLE_W/2, x+GRILLE_W/2, GRILLE_Y[0], GRILLE_Y[1], 23.0, 28.0)
    gcut = s if gcut is None else gcut.union(s)
cov = trimesh.load(COVER_IN, force='mesh'); cov.merge_vertices()
cres = trimesh.boolean.difference([cov, to_mesh(gcut, "cover_cut.stl")])
cres = finalize(cres, COVER_OUT)

with open(os.path.join(HERE, "hang.json"), "w") as f:
    json.dump({"HANG_T": HANG_T, "PI_T": PI_T}, f, indent=1)

for n, m in (("case", res), ("cover", cres)):
    print("%-6s watertight=%s  winding=%s  vol=%.1f cm3  bbox=%s" % (
        n, m.is_watertight, m.is_winding_consistent, m.volume/1000,
        [[round(float(v), 1) for v in r] for r in m.bounds]))

# OCP (OpenCASCADE) segfaults at interpreter teardown on Windows *after* all work is done;
# exit explicitly so the pipeline gets a truthful status.
import sys; sys.stdout.flush(); os._exit(0)
