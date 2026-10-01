#include "pigaze/linalg.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace pigaze {

bool solveLinear(std::vector<double>& A, std::vector<double>& b, int n) {
    double scale = 0;
    for (double v : A) scale = std::max(scale, std::fabs(v));
    if (scale == 0) return false;
    const double eps = 1e-12 * scale;
    for (int c = 0; c < n; ++c) {
        int piv = c;
        for (int r = c + 1; r < n; ++r)
            if (std::fabs(A[r * n + c]) > std::fabs(A[piv * n + c])) piv = r;
        if (std::fabs(A[piv * n + c]) < eps) return false;
        if (piv != c) {
            for (int k = 0; k < n; ++k) std::swap(A[c * n + k], A[piv * n + k]);
            std::swap(b[c], b[piv]);
        }
        for (int r = c + 1; r < n; ++r) {
            double f = A[r * n + c] / A[c * n + c];
            if (f == 0) continue;
            for (int k = c; k < n; ++k) A[r * n + k] -= f * A[c * n + k];
            b[r] -= f * b[c];
        }
    }
    for (int r = n - 1; r >= 0; --r) {
        double s = b[r];
        for (int k = r + 1; k < n; ++k) s -= A[r * n + k] * b[k];
        b[r] = s / A[r * n + r];
    }
    return true;
}

bool leastSquares(const std::vector<double>& X, const std::vector<double>& y, int m, int n,
                  std::vector<double>& coef, double ridge, const std::vector<bool>& unpenalised) {
    if (m < 1 || n < 1) return false;
    std::vector<double> A(static_cast<size_t>(n) * n, 0.0), b(n, 0.0);
    for (int i = 0; i < m; ++i) {
        const double* row = &X[static_cast<size_t>(i) * n];
        for (int j = 0; j < n; ++j) {
            b[j] += row[j] * y[i];
            for (int k = 0; k < n; ++k) A[j * n + k] += row[j] * row[k];
        }
    }
    if (ridge > 0)
        for (int j = 0; j < n; ++j)
            if (unpenalised.empty() || !unpenalised[j]) A[j * n + j] += ridge;
    if (!solveLinear(A, b, n)) return false;
    coef = b;
    return true;
}

}  // namespace pigaze
