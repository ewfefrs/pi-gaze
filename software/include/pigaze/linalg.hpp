// Pi-Gaze — tiny dense linear least squares helpers.
#pragma once
#include <vector>

namespace pigaze {

// Solves A x = b in place (A is n*n row-major, b has n entries; x is returned in b).
// Gaussian elimination with partial pivoting. Returns false if A is (near) singular.
bool solveLinear(std::vector<double>& A, std::vector<double>& b, int n);

// Least squares: rows of X (m*n row-major) against y (m). ridge > 0 regularises all
// coefficients except those flagged in `unpenalised` (may be empty).
bool leastSquares(const std::vector<double>& X, const std::vector<double>& y, int m, int n,
                  std::vector<double>& coef, double ridge = 0.0,
                  const std::vector<bool>& unpenalised = {});

}  // namespace pigaze
