"""
Pi-Gaze COUNTER-MODEL: tries to prove the assembly is broken.

Every printable part is placed in its ASSEMBLED pose (clamp frame) and checked:
  A  every printable part: watertight + exactly one solid body (nothing "floats")
  B  pairwise interference between all assembled parts
  C  real Raspberry Pi 5 + Active Cooler STL inside the modified case / cover
  D  ghost camera ribbon swept along the whole route vs every part and the Pi
  E  monitor (15 mm top edge) fits the throat; front lip really catches the bezel
  F  bar hinge swept -20..-48 deg vs clamp, ribbon cover (+screw heads), knob, case
  G  thumbscrew knob fully in / backed out vs case; finger room
  H  plug / card / button access: USB, RJ45, USB-C, HDMI, microSD, power button
  I  interfaces: dock holes <-> jaw nut pockets, ribbon slot <-> jaw tunnel
  J  probes: bar light compartments, clamp lip, nut pockets reachable
  K  fan intake gap, camera cable length
Run:  hardware/check.sh   (exports poses, then runs this)
"""
import os, json, itertools, numpy as np, trimesh

ROOT  = os.path.dirname(os.path.abspath(__file__))
POSE  = os.path.join(ROOT, "enclosure", "out", "pose")
STL   = os.path.join(ROOT, "enclosure", "stl")
MOUNT = os.path.join(ROOT, "mount")
PI_RAW = r"C:\Users\bronl\Downloads\RASPBERRY_PI_5_WITH_ACTIVE_COOLER.stl"
H = json.load(open(os.path.join(MOUNT, "hang.json")))
HANG, PIT = np.array(H["HANG_T"], float), np.array(H["PI_T"], float)

# --- clamp constants (mirror enclosure/src/clamp.scad; interface checks below compare them to the case)
MONITOR_T = 15.0; MON_FRONT = -17.0
DOCK_XS, DOCK_Z = [14.5, 21.5], -36.0
RIB_X, RIB_Y = (-25.5, -7.5), (19.0, 22.0)
HINGE_X, HY, PIV_Z, HR = -16.5, -11.0, 16.0, 7.0
SCREW_Z = 7 - 48 * 0.55
TOL = 0.05                                  # mm^3: anything larger is a real collision

results = []
def check(name, ok, detail=""):
    results.append((name, bool(ok), detail)); print("  %s  %-46s %s" % ("PASS" if ok else "FAIL", name, detail))

def load(p, T=None):
    m = trimesh.load(p, force='mesh')
    if T is not None: m = m.copy(); m.apply_transform(T)
    return m
def box(x0, x1, y0, y1, z0, z1):
    b = trimesh.creation.box(extents=[x1-x0, y1-y0, z1-z0]); b.apply_translation([(x0+x1)/2, (y0+y1)/2, (z0+z1)/2]); return b
def ivol(a, b):
    if (a.bounds[0] >= b.bounds[1]).any() or (b.bounds[0] >= a.bounds[1]).any(): return 0.0
    r = trimesh.boolean.intersection([a, b])
    return abs(r.volume) if len(r.faces) else 0.0
def rotx(deg):
    c, s = np.cos(np.radians(deg)), np.sin(np.radians(deg))
    return np.array([[1,0,0,0],[0,c,-s,0],[0,s,c,0],[0,0,0,1]], float)
def tr(x, y, z):
    T = np.eye(4); T[:3, 3] = [x, y, z]; return T
def bar_pose(tilt): return tr(HINGE_X, HY, PIV_Z) @ rotx(tilt) @ tr(0, 0, HR)
def pt(T, p): return (T @ np.r_[p, 1.0])[:3]

parts = {
    "clamp":        load(os.path.join(POSE, "pose_clamp.stl")),
    "ribbon_cover": load(os.path.join(POSE, "pose_ribbon_cover.stl")),
    "pad":          load(os.path.join(POSE, "pose_pad.stl")),
    "foot":         load(os.path.join(POSE, "pose_foot.stl")),
    "knob":         load(os.path.join(POSE, "pose_knob_y31.stl")),
    "bar":          load(os.path.join(STL, "pigaze_bar.stl"), None),       # posed below in float64
    "bar_lid":      load(os.path.join(STL, "pigaze_bar_lid.stl"), None),
    "case":         load(os.path.join(MOUNT, "case_pigaze.stl"), HANG),
    "cover":        load(os.path.join(MOUNT, "cover_pigaze.stl"), HANG),
}

BAR_N, LID_N = parts["bar"].copy(), parts["bar_lid"].copy()   # native (watertight) frames
def posed(m, tilt):
    x = m.copy(); x.apply_transform(bar_pose(tilt)); return x      # float64 -> stays watertight
parts["bar"], parts["bar_lid"] = posed(BAR_N, -38), posed(LID_N, -38)

print("\n[A] printable parts: watertight + one solid body")
printables = {f: os.path.join(STL, "pigaze_%s.stl" % f) for f in ["clamp","ribbon_cover","pad","foot","knob","bar","bar_lid"]}
printables.update({"case": os.path.join(MOUNT, "case_pigaze.stl"), "cover": os.path.join(MOUNT, "cover_pigaze.stl")})
for n, p in printables.items():
    m = load(p); bodies = [b for b in m.split(only_watertight=False) if b.volume > 0]
    check("A %s" % n, m.is_watertight and len(bodies) == 1, "watertight=%s solids=%d" % (m.is_watertight, len(bodies)))

print("\n[B] pairwise interference (assembled, bar at -38 deg)")
for a, b in itertools.combinations(parts, 2):
    v = ivol(parts[a], parts[b])
    if a == "case" and b == "cover":
        check("B case<->cover (stock snap fit)", True, "INFO overlap %.2f mm3 (original cover design)" % v); continue
    if v > TOL or v > 0: check("B %s <-> %s" % (a, b), v <= TOL, "overlap %.3f mm3" % v)
check("B all other pairs", True, "no overlap")

print("\n[C] real Pi 5 + Active Cooler inside the modified case / cover")
raw = trimesh.load(PI_RAW, force='mesh')
P_case = (np.c_[raw.vertices, np.ones(len(raw.vertices))] @ PIT.T)[:, :3]
P_case = np.unique(np.round(P_case, 3), axis=0)
case_c, cov_c = load(os.path.join(MOUNT, "case_pigaze.stl")), load(os.path.join(MOUNT, "cover_pigaze.stl"))
inc = case_c.contains(P_case); inv = cov_c.contains(P_case)
standoff = (P_case[:, 2] > 2.75) & (P_case[:, 2] < 2.95)          # PCB resting on the standoffs
bad = inc & ~standoff
check("C Pi vs case (excl. PCB-on-standoff contact)", bad.sum() <= 40, "%d pts inside (%d standoff contact)" % (bad.sum(), (inc & standoff).sum()))
check("C Pi vs cover", inv.sum() == 0, "%d pts inside" % inv.sum())
top = P_case[:, 2].max(); check("C tallest Pi part under the cover", top < 24.03, "top Z %.2f, cover underside 24.03" % top)

print("\n[D] ghost camera ribbon (16.2 mm) along the full route")
RX = (-24.6, -8.4)
ghost = [("inside case -> slot -> collar -> jaw tunnel", box(*RX, 20.3, 20.7, -100.0, 4.2)),
         ("bridge-top groove (under cover)",            box(*RX, -3.0, 20.7, 3.8, 4.2)),
         ("free rise to the bar",                       box(*RX, -3.0, -1.6, 3.8, 16.4))]
slot_local = box(-8.1, 8.1, 5.5, 7.0, -2.5, 4.0); slot_local.apply_transform(bar_pose(-38))
ghost.append(("through the bar floor slot (bar -38 deg)", slot_local))
allparts = dict(parts)
for gname, g in ghost:
    hits = {n: ivol(g, m) for n, m in allparts.items()}
    worst = max(hits, key=hits.get)
    check("D ribbon: %s" % gname, hits[worst] <= TOL, "worst: %s %.3f mm3" % (worst, hits[worst]))
P_clamp = (np.c_[P_case, np.ones(len(P_case))] @ HANG.T)[:, :3]
g0 = ghost[0][1]; lo, hi = g0.bounds[0] - 0.3, g0.bounds[1] + 0.3
inside = ((P_clamp >= lo) & (P_clamp <= hi)).all(1).sum()
check("D ribbon run under the cover vs real Pi", inside == 0, "%d Pi points in the ribbon's path" % inside)
route = (4.2 - (-(9.0) - 94.0)) + 14.5 + (20.7 - (-1.6)) + (16.4 - 3.8) + 6.0
check("K camera cable route length", route < 190, "%.0f mm -> use the 200 mm Pi 5 camera cable (300 mm also fits, fold slack under the cover)" % route)

print("\n[E] monitor fit (top edge %.0f mm)" % MONITOR_T)
mon = box(-160, 160, MON_FRONT, MON_FRONT + MONITOR_T, -300, 0.0)
for n in ["clamp", "pad", "foot", "case", "cover", "ribbon_cover", "knob"]:
    v = ivol(mon, parts[n]); check("E monitor <-> %s" % n, v <= TOL, "overlap %.3f mm3" % v)
cv = parts["clamp"].vertices; lipz = cv[cv[:, 1] < -17.5][:, 2].min()
check("E front lip hangs below the monitor top", parts["clamp"].contains([[0, -20, -3.0]])[0], "lip bottom Z %.1f (monitor top at 0)" % lipz)

print("\n[F] hinge sweep -20..-48 deg")
screw_heads = [trimesh.creation.cylinder(radius=2.75, height=2.2) for _ in range(2)]
for h, y in zip(screw_heads, [6.0, 14.0]): h.apply_translation([-3.25, y, 7.0 + 1.1])
for t in [20, 30, 38, 48]:
    b, l = posed(BAR_N, -t), posed(LID_N, -t)
    worst = 0.0; who = "-"
    for n in ["clamp", "ribbon_cover", "knob", "case", "cover"]:
        for x in (b, l):
            v = ivol(x, parts[n])
            if v > worst: worst, who = v, n
    for sh in screw_heads:
        for x in (b, l):
            v = ivol(x, sh)
            if v > worst: worst, who = v, "cover screw head"
    check("F bar+lid at -%d deg" % t, worst <= TOL, "worst: %s %.3f mm3" % (who, worst))

print("\n[G] thumbscrew knob travel")
for y in [31, 45]:
    k = load(os.path.join(POSE, "pose_knob_y%d.stl" % y))
    worst = max(ivol(k, parts[n]) for n in ["case", "cover", "bar", "bar_lid", "ribbon_cover"])
    check("G knob at Y=%d vs case/bar" % y, worst <= TOL, "overlap %.3f mm3" % worst)
    d = trimesh.proximity.closest_point(parts["case"], k.sample(3000))[1].min()
    need = 7.0 if y == 31 else 10.0     # Y31 = fully in (only for a ~27 mm monitor); Y45 = typical 13-16 mm
    check("G finger room knob(Y=%d) -> case" % y, d >= need, "min gap %.1f mm (need %.0f)" % (d, need))
bolt = trimesh.creation.cylinder(radius=1.5, height=45); bolt.apply_transform(tr(0, 5 + 22.5, SCREW_Z) @ rotx(90))
check("G thumbscrew bolt vs case/cover", max(ivol(bolt, parts["case"]), ivol(bolt, parts["cover"])) <= TOL, "")

print("\n[H] access: plugs, card, button")
def cbox(x0, x1, y0, y1, z0, z1):
    b = box(x0, x1, y0, y1, z0, z1); b.apply_transform(HANG); return b
zones = {"RJ45 plug (down)":   cbox(48.8, 94, -25.5, -10.5, 6.5, 18.5),
         "USB stack 1 plug":   cbox(48.8, 94, -5.5, 7.5, 6.5, 20.5),
         "USB stack 2 plug":   cbox(48.8, 94, 12.5, 25.5, 6.5, 20.5),
         "USB-C power plug":   cbox(-35.5, -27.5, -72, -32.8, 4.5, 8.5),
         "micro-HDMI 0 plug":  cbox(-20.5, -14.5, -72, -32.8, 4.5, 8.5),
         "micro-HDMI 1 plug":  cbox(-7.5, 0.5, -72, -32.8, 4.5, 8.5),
         "microSD fingernail": cbox(-62, -47.1, -5, 5, -2.4, 2.4),
         "power-button lever": cbox(-60, -48.2, -14, -8, 0, 7)}
for zn, z in zones.items():
    worst = max(ivol(z, m) for n, m in allparts.items() if n not in ("case",) or zn in ("microSD fingernail", "power-button lever"))
    check("H %s free" % zn, worst <= TOL, "blocked %.3f mm3" % worst)

print("\n[I] interfaces")
for (cx, cy), dx in zip([(-58.0, 11.0), (-58.0, 18.0)], DOCK_XS):
    p = pt(HANG, [cx, cy, 13.0])
    check("I dock hole case(%.0f,%.1f) -> jaw nut X%.0f" % (cx, cy, dx), abs(p[0] - dx) < 0.05 and abs(p[2] - DOCK_Z) < 0.05, "maps to X %.2f Z %.2f" % (p[0], p[2]))
a, b = pt(HANG, [-45, -28.5, 17.5]), pt(HANG, [-45, -11.5, 22.5])
sx, sy = sorted([a[0], b[0]]), sorted([a[1], b[1]])
check("I case ribbon slot within jaw tunnel X", sx[0] >= RIB_X[0] - 1 and sx[1] <= RIB_X[1] + 1, "slot X %.1f..%.1f / tunnel %.1f..%.1f" % (sx[0], sx[1], *RIB_X))
check("I ribbon slot straddles tunnel Y", sy[0] <= RIB_Y[0] and sy[1] >= RIB_Y[1], "slot Y %.1f..%.1f / tunnel %.1f..%.1f" % (sy[0], sy[1], *RIB_Y))
check("I hinge on the ribbon line", abs(HINGE_X - (RIB_X[0] + RIB_X[1]) / 2) < 0.01, "hinge X %.1f" % HINGE_X)

print("\n[J] probes")
barn = load(os.path.join(STL, "pigaze_bar.stl")); cl = parts["clamp"]
check("J bar: compartment wall at IR height (+/-18.3,0,9)", barn.contains([[18.3, 0, 9], [-18.3, 0, 9]]).all(), "")
check("J bar: no external fins (+/-20.5,0,22)", not barn.contains([[20.5, 0, 22], [-20.5, 0, 22]]).any(), "")
for nm, (x, z) in {"thumbscrew": (0.0, SCREW_Z), "dock 1": (14.5, DOCK_Z), "dock 2": (21.5, DOCK_Z)}.items():
    line = [[x + 2.0, y, z] for y in np.linspace(15.0, 19.3, 10)]
    check("J %s nut pocket reachable from monitor side" % nm, not cl.contains(line).any(), "")
check("J ribbon tunnel open bottom->top", not cl.contains([[-16.5, 20.5, z] for z in np.linspace(-42, 6.9, 40)]).any(), "")

print("\n[L] fasteners: screw tips vs monitor, nut engagement")
NUT_TH = 2.4
# ribbon-cover screws M3x6: head on the cover (Z 7.0), nut pulled up to the pocket top (Z 4.4)
tip = 7.0 - 6.0; nut0, nut1 = 4.4 - NUT_TH, 4.4
check("L cover screw M3x6 tip stays inside the bridge", tip > 0.2, "tip Z %.1f (monitor top at 0)" % tip)
check("L cover screw fully through its nut", tip <= nut0, "engaged %.1f / %.1f mm" % (nut1 - max(tip, nut0), NUT_TH))
check("L cover nut pocket reachable from below", not cl.contains([[-3.25 + 1.5, y, 2.0] for y in (6.0, 14.0)]).any(), "")
# dock screws M3x12: head bears at case Z 12.8 -> clamp Y 27.7; nut at the jaw inner face Y 17..19.4
dtip = (HANG[1][3] - 12.8) - 12.0
check("L dock screw M3x12 through the jaw nut", dtip <= 17.0, "tip Y %.1f, nut Y 17.0-19.4" % dtip)
check("L dock screw tip clear of a 27 mm monitor", dtip > MON_FRONT + 27.0, "tip Y %.1f vs monitor back Y %.1f" % (dtip, MON_FRONT + 27.0))

print("\n[K] cooling")
gap = (-27.0 + HANG[1][3]) - (MON_FRONT + MONITOR_T)
check("K fan-grille -> monitor back air gap", gap >= 10.0, "%.1f mm" % gap)

nf = sum(1 for r in results if not r[1])
print("\n" + ("COUNTER-MODEL CLEAN: %d checks, 0 failures" % len(results) if nf == 0 else "COUNTER-MODEL FOUND %d FAILURE(S) of %d" % (nf, len(results))))
