// Dark-pupil detection for an IR-lit eye:
//   bright blobs -> inpaint glints -> darkest seed -> dark core -> pupil/iris levels ->
//   region at the mid level -> radial edge points -> ellipse fit -> confidence.
#include "pigaze/eye_detector.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "pigaze/linalg.hpp"

namespace pigaze {
namespace {

struct Blob {
    int area = 0;
    double sw = 0, sx = 0, sy = 0;   // sums weighted by brightness above the threshold
    std::vector<int> pixels;
};

// 8-connected blobs of pixels >= thr.
std::vector<Blob> brightBlobs(const Image& img, int thr) {
    const int W = img.width, H = img.height;
    std::vector<uint8_t> seen(img.px.size(), 0);
    std::vector<int> stack;
    std::vector<Blob> blobs;
    for (int i = 0; i < W * H; ++i) {
        if (seen[i] || img.px[i] < thr) continue;
        Blob b;
        stack.assign(1, i);
        seen[i] = 1;
        while (!stack.empty()) {
            int j = stack.back();
            stack.pop_back();
            int x = j % W, y = j / W;
            double w = img.px[j] - thr + 1.0;
            b.area++;
            b.sw += w; b.sx += w * x; b.sy += w * y;
            b.pixels.push_back(j);
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                    int k = ny * W + nx;
                    if (!seen[k] && img.px[k] >= thr) { seen[k] = 1; stack.push_back(k); }
                }
        }
        blobs.push_back(std::move(b));
    }
    return blobs;
}

// Masked pixels take the mean of the nearest unmasked neighbours.
void inpaint(Image& img, const std::vector<uint8_t>& mask) {
    const Image src = img;
    const int W = img.width, H = img.height;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            if (!mask[static_cast<size_t>(y) * W + x]) continue;
            for (int k = 1; k <= 8; ++k) {
                int s = 0, n = 0;
                for (int yy = std::max(0, y - k); yy <= std::min(H - 1, y + k); ++yy)
                    for (int xx = std::max(0, x - k); xx <= std::min(W - 1, x + k); ++xx)
                        if (!mask[static_cast<size_t>(yy) * W + xx]) { s += src.at(xx, yy); ++n; }
                if (n >= 3) { img.at(x, y) = static_cast<uint8_t>((s + n / 2) / n); break; }
            }
        }
}

// 4-connected region of pixels <= thr grown from `seed`. False if it leaks (too big or
// reaches the window border — a pupil never touches the border of a centred window).
bool growDark(const Image& img, int seed, int thr, int maxArea, std::vector<int>& region) {
    const int W = img.width, H = img.height;
    region.clear();
    if (img.px[seed] > thr) return false;
    std::vector<uint8_t> seen(img.px.size(), 0);
    std::vector<int> stack{seed};
    seen[seed] = 1;
    static const int nb[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    while (!stack.empty()) {
        int j = stack.back();
        stack.pop_back();
        region.push_back(j);
        if (static_cast<int>(region.size()) > maxArea) return false;
        int x = j % W, y = j / W;
        if (x == 0 || y == 0 || x == W - 1 || y == H - 1) return false;
        for (const auto& d : nb) {
            int k = (y + d[1]) * W + (x + d[0]);
            if (!seen[k] && img.px[k] <= thr) { seen[k] = 1; stack.push_back(k); }
        }
    }
    return true;
}

struct Shape { double cx = 0, cy = 0, a = 0, b = 0, area = 0; };   // a >= b: semi-axes

Shape shapeOf(const std::vector<int>& region, int W) {
    Shape s;
    const double n = static_cast<double>(region.size());
    if (n == 0) return s;
    double sx = 0, sy = 0;
    for (int j : region) { sx += j % W; sy += j / W; }
    s.cx = sx / n; s.cy = sy / n;
    double m20 = 0, m02 = 0, m11 = 0;
    for (int j : region) {
        double dx = j % W - s.cx, dy = j / W - s.cy;
        m20 += dx * dx; m02 += dy * dy; m11 += dx * dy;
    }
    m20 = m20 / n + 1.0 / 12; m02 = m02 / n + 1.0 / 12; m11 /= n;   // + pixel's own extent
    double c = std::sqrt(0.25 * (m20 - m02) * (m20 - m02) + m11 * m11);
    s.a = 2 * std::sqrt(0.5 * (m20 + m02) + c);
    s.b = 2 * std::sqrt(std::max(0.0, 0.5 * (m20 + m02) - c));
    s.area = n;
    return s;
}

// Lower quartile of the pixels in a ring (biased to the iris, not the sclera/skin).
double ringLevel(const Image& img, const std::vector<uint8_t>& excl, double cx, double cy,
                 double r0, double r1) {
    std::vector<uint8_t> v;
    int x0 = std::max(0, static_cast<int>(cx - r1)), x1 = std::min(img.width - 1, static_cast<int>(cx + r1) + 1);
    int y0 = std::max(0, static_cast<int>(cy - r1)), y1 = std::min(img.height - 1, static_cast<int>(cy + r1) + 1);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            double d = std::hypot(x - cx, y - cy);
            if (d >= r0 && d < r1 && !excl[static_cast<size_t>(y) * img.width + x]) v.push_back(img.at(x, y));
        }
    if (v.size() < 8) return -1;
    auto q = v.begin() + v.size() / 4;
    std::nth_element(v.begin(), q, v.end());
    return *q;
}

struct Conic { Vec2 c; double A = 0, B = 0, C = 0, K = 0, ra = 0, rb = 0; };

// A x^2 + B xy + C y^2 + D x + E y = 1 through points around an interior origin.
bool fitEllipse(const std::vector<Vec2>& pts, Conic& e) {
    const int m = static_cast<int>(pts.size());
    if (m < 6) return false;
    double s = 0;
    for (const Vec2& p : pts) s += p.norm();
    if (s <= 0) return false;
    s = m / s;                                          // scale points to ~unit radius
    std::vector<double> X(static_cast<size_t>(m) * 5), y(m, 1.0), c;
    for (int i = 0; i < m; ++i) {
        double px = pts[i].x * s, py = pts[i].y * s;
        double* row = &X[static_cast<size_t>(i) * 5];
        row[0] = px * px; row[1] = px * py; row[2] = py * py; row[3] = px; row[4] = py;
    }
    if (!leastSquares(X, y, m, 5, c)) return false;
    const double A = c[0], B = c[1], C = c[2], D = c[3], E = c[4];
    const double den = 4 * A * C - B * B;
    if (den <= 1e-12) return false;                     // not an ellipse
    const double x0 = (B * E - 2 * C * D) / den, y0 = (B * D - 2 * A * E) / den;
    const double K = A * x0 * x0 + B * x0 * y0 + C * y0 * y0 + D * x0 + E * y0 - 1;
    const double tr = A + C, dd = std::sqrt((A - C) * (A - C) + B * B);
    const double l1 = 0.5 * (tr + dd), l2 = 0.5 * (tr - dd);
    if (l1 <= 0 || l2 <= 0 || K >= 0) return false;
    e.ra = std::sqrt(-K / l2) / s;
    e.rb = std::sqrt(-K / l1) / s;
    e.c = {x0 / s, y0 / s};
    e.A = A * s * s; e.B = B * s * s; e.C = C * s * s; e.K = K;   // quadratic form in px
    return true;
}

// Radial distance of p outside (+) / inside (-) the fitted ellipse.
double radialResidual(const Conic& e, const Vec2& p) {
    const Vec2 u = p - e.c;
    const double q = e.A * u.x * u.x + e.B * u.x * u.y + e.C * u.y * u.y;
    if (q <= 0) return 0;
    const double s = std::sqrt(q / -e.K);               // 1 on the boundary
    return u.norm() * (1 - 1 / s);
}

// x^2 + y^2 + D x + E y + F = 0 (Kasa).
bool fitCircle(const std::vector<Vec2>& pts, Vec2& center, double& r) {
    const int m = static_cast<int>(pts.size());
    if (m < 3) return false;
    std::vector<double> X(static_cast<size_t>(m) * 3), y(m), c;
    for (int i = 0; i < m; ++i) {
        X[i * 3] = pts[i].x; X[i * 3 + 1] = pts[i].y; X[i * 3 + 2] = 1;
        y[i] = -(pts[i].x * pts[i].x + pts[i].y * pts[i].y);
    }
    if (!leastSquares(X, y, m, 3, c)) return false;
    center = {-c[0] / 2, -c[1] / 2};
    double r2 = center.x * center.x + center.y * center.y - c[2];
    if (r2 <= 0) return false;
    r = std::sqrt(r2);
    return true;
}

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

}  // namespace

EyeMeasurement detectEye(const ImageView& frame, Vec2 center, const DetectorParams& p,
                         double glintMaxDist) {
    EyeMeasurement m;
    const Rect roi = Rect::centered(center, p.roiSize).clipped(frame.width, frame.height);
    m.roi = roi;
    if (roi.w < 16 || roi.h < 16) return m;
    const Image raw = crop(frame, roi);
    const int W = raw.width, H = raw.height;
    const int med = medianOf(raw);
    const int gthr = std::min(254, std::max(p.glintMin, med + p.glintDelta));

    // 1. bright blobs; the small ones (glints) are masked and inpainted so they cannot
    //    cut holes into the pupil
    const std::vector<Blob> blobs = brightBlobs(raw, gthr);
    std::vector<uint8_t> mask(raw.px.size(), 0);
    for (const Blob& b : blobs) {
        if (b.area > 4 * p.glintMaxArea) continue;
        for (int j : b.pixels) {
            int x = j % W, y = j / W;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && ny >= 0 && nx < W && ny < H) mask[static_cast<size_t>(ny) * W + nx] = 1;
                }
        }
    }
    Image clean = raw;
    inpaint(clean, mask);
    Image fine, coarse;
    boxBlur(clean, fine, 1);
    boxBlur(clean, coarse, std::max(1, static_cast<int>(std::lround(p.pupilMinR))));

    // 2. seed: darkest coarse pixel, mildly pulled towards the window centre
    const double lcx = center.x - roi.x, lcy = center.y - roi.y;
    const double searchR = 0.36 * p.roiSize;
    int seed = -1;
    double best = 1e18;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            double d = std::hypot(x - lcx, y - lcy);
            if (d > searchR) continue;
            double s = coarse.at(x, y) + 0.6 * d;
            if (s < best) { best = s; seed = y * W + x; }
        }
    if (seed < 0) return m;
    const int vmin = coarse.px[seed];
    if (med - vmin < 12) return m;                        // nothing dark enough here

    const int maxArea = static_cast<int>(kPi * (p.pupilMaxR + 2) * (p.pupilMaxR + 2));
    std::vector<int> region;

    // 3. dark core (only clearly-pupil pixels)
    const int thr1 = vmin + std::max(6, static_cast<int>((med - vmin) * 0.15));
    if (!growDark(coarse, seed, thr1, maxArea, region)) return m;
    const Shape core = shapeOf(region, W);
    const double r1 = std::sqrt(core.area / kPi);

    // 4. pupil level inside the core, iris level in a ring around it
    double pupilLevel = 0;
    for (int j : region) pupilLevel += clean.px[j];
    pupilLevel /= static_cast<double>(region.size());
    double iris = ringLevel(clean, mask, core.cx, core.cy, 1.25 * r1 + 1.5, 1.9 * r1 + 3.0);
    if (iris < 0) iris = med;
    const double contrast = iris - pupilLevel;
    if (contrast < 8) return m;

    // 5. pupil = region below the pupil/iris mid level (fallback: lower level if it leaks)
    int fseed = region[0];
    for (int j : region) if (fine.px[j] < fine.px[fseed]) fseed = j;
    int thr2 = static_cast<int>(std::lround(pupilLevel + 0.5 * contrast));
    bool ok = growDark(fine, fseed, thr2, maxArea, region);
    if (!ok) {
        thr2 = static_cast<int>(std::lround(pupilLevel + 0.3 * contrast));
        ok = growDark(fine, fseed, thr2, maxArea, region);
    }
    if (!ok) return m;
    const Shape sh = shapeOf(region, W);
    const double rMom = std::sqrt(sh.a * std::max(sh.b, 1e-6));

    // 6. radial edge points at the same mid level, skipping rays near glints
    const int rays = 48;
    const double tMax = std::min(2.2 * rMom + 4.0, 0.5 * p.roiSize);
    std::vector<Vec2> edge;
    std::vector<double> edgeT;
    for (int k = 0; k < rays; ++k) {
        const double ang = 2 * kPi * k / rays, dx = std::cos(ang), dy = std::sin(ang);
        double prev = bilinear(fine, sh.cx, sh.cy);
        if (prev >= thr2) break;                          // centroid not dark: shape is odd
        for (double t = 0.25; t <= tMax; t += 0.25) {
            const double v = bilinear(fine, sh.cx + dx * t, sh.cy + dy * t);
            if (v >= thr2) {
                const double te = t - 0.25 + 0.25 * (thr2 - prev) / std::max(v - prev, 1e-6);
                const int ex = static_cast<int>(std::lround(sh.cx + dx * te));
                const int ey = static_cast<int>(std::lround(sh.cy + dy * te));
                bool nearGlint = false;
                for (int yy = ey - 1; yy <= ey + 1 && !nearGlint; ++yy)
                    for (int xx = ex - 1; xx <= ex + 1; ++xx)
                        if (xx >= 0 && yy >= 0 && xx < W && yy < H && mask[static_cast<size_t>(yy) * W + xx]) {
                            nearGlint = true;
                            break;
                        }
                if (!nearGlint) { edge.push_back({dx * te, dy * te}); edgeT.push_back(te); }
                break;
            }
            prev = v;
        }
    }
    std::vector<Vec2> inl;
    if (!edgeT.empty()) {
        std::vector<double> t = edgeT;
        std::nth_element(t.begin(), t.begin() + t.size() / 2, t.end());
        const double medT = t[t.size() / 2];
        for (size_t i = 0; i < edge.size(); ++i)
            if (std::fabs(edgeT[i] - medT) <= 0.2 * medT + 0.75) inl.push_back(edge[i]);
    }

    // 7. centre: ellipse fit -> circle fit -> centroid. Eyelids and lashes can only cut INTO
    //    the pupil, never extend it: edge points well inside the fit are occluder edges, so
    //    they are dropped and the fit repeated.
    Vec2 c(sh.cx, sh.cy);
    double radius = rMom;
    for (int iter = 0; iter < 5; ++iter) {
        Conic e;
        Vec2 off;
        double rc = 0;
        std::vector<double> res;
        bool fitted = false;
        if (inl.size() >= 10 && fitEllipse(inl, e) && e.c.norm() < 0.8 * rMom && e.rb > 0.4 * rMom &&
            e.ra < 2.0 * rMom + 1) {
            fitted = true;
            c = Vec2(sh.cx, sh.cy) + e.c;
            radius = std::sqrt(e.ra * e.rb);
            for (const Vec2& q : inl) res.push_back(radialResidual(e, q));
        } else if (inl.size() >= 6 && fitCircle(inl, off, rc) && off.norm() < 0.8 * rMom &&
                   rc > 0.5 * rMom && rc < 2.0 * rMom + 1) {
            fitted = true;
            c = Vec2(sh.cx, sh.cy) + off;
            radius = rc;
            for (const Vec2& q : inl) res.push_back(dist(q, off) - rc);
        }
        if (!fitted) break;
        std::vector<Vec2> keep;
        for (size_t i = 0; i < inl.size(); ++i)
            if (res[i] >= -0.6) keep.push_back(inl[i]);
        if (keep.size() == inl.size() || keep.size() < 8) break;
        inl.swap(keep);
    }

    // 8. confidence
    const double cContrast = clamp01((contrast - 8) / 40.0);
    const double cShape = clamp01((sh.b / sh.a - 0.35) / 0.45);
    const double fill = sh.area / (kPi * sh.a * std::max(sh.b, 1e-6));
    const double cFill = clamp01(1 - 2.5 * std::fabs(fill - 1));
    const double cEdge = clamp01((static_cast<double>(inl.size()) / rays - 0.3) / 0.5);
    m.confidence = 0.35 * cContrast + 0.25 * cShape + 0.2 * cEdge + 0.2 * cFill;
    m.radius = radius;
    m.pupil = {c.x + roi.x, c.y + roi.y};
    m.valid = m.confidence >= p.minConfidence && radius >= p.pupilMinR && radius <= p.pupilMaxR;
    if (!m.valid) return m;

    // 9. glints: small bright blobs close to the pupil, merged into one weighted centroid
    const double gmax = glintMaxDist > 0 ? glintMaxDist : std::max(4.0 * radius, 10.0);
    double sw = 0, sx = 0, sy = 0;
    int cnt = 0;
    for (const Blob& b : blobs) {
        if (b.area > p.glintMaxArea) continue;
        if (dist({b.sx / b.sw, b.sy / b.sw}, c) > gmax) continue;
        sw += b.sw; sx += b.sx; sy += b.sy; ++cnt;
    }
    if (cnt > 0) {
        m.hasGlint = true;
        m.glint = {sx / sw + roi.x, sy / sw + roi.y};
        m.glintCount = cnt;
    }
    return m;
}

}  // namespace pigaze
