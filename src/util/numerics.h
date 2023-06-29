#pragma once

/**
 * Basic numeric helpers.
 * Root finding and integration in one dimension.
 */

#include <functional>
#include <stdexcept>

namespace util {

/**
 * Function type used throughout this module.
 * (std::function is not the most performant choice but it is the simplest)
 */
typedef std::function<double(double)> function_t;

/** exception thrown if some method does not converge */
class numerics_exception : std::runtime_error
{
  public:
	numerics_exception(const std::string &what_arg)
	    : std::runtime_error(what_arg)
	{}

	numerics_exception(const char *what_arg) : std::runtime_error(what_arg) {}
};

/**
 * Solve f(x) = 0 for x in [a,b].
 * Implemented using Secant method with fallback to bisection.
 *    - f(a) and f(b) need to have different signs
 *    - result will be precise to full machine precision
 *    - will throw if no reliable result can be obtained.
 */
double solve(function_t f, double a, double b);

/**
 * Integrate f(x) for x in [a, b].
 * Implemented using adaptive Gauss-Kronrod quadrature.
 *   - 'eps' is the target relative error
 *     (the used estimate is very pessimistic for most somewhat nice functions)
 *   - throws numerics_exception if not converged within maxCalls
 */
double integrate(function_t f, double a, double b);
double integrate(function_t f, double a, double b, double eps, int maxCalls);

/**
 * Integrate f(x) for x in (-∞,∞).
 * Implemented using Gauss-Hermite quadrature
 *   - assumes f(x) ~ exp(-x^r) for large values of |x|, with r = 2,4,6,...
 */
double integrate_hermite_15(function_t f);
double integrate_hermite_31(function_t f);
double integrate_hermite_63(function_t f);

// sum of (double precision) floating point numbers without rounding
class FSum
{
	std::vector<double> parts_;

  public:
	FSum() = default;
	explicit FSum(double x);

	// add/subtract x to the current sum
	FSum &operator+=(double x);
	FSum &operator-=(double x);

	// double-precision approximation to current sum
	explicit operator double() const;

	// returns and subtracts double-precision approximation
	double subtract_double();

	// debugging only
	std::vector<double> const &parts() const { return parts_; }
};

// sum xs values, only rounding once. Equivalent to python's 'math.fsum'
inline double fsum(std::span<const double> xs)
{
	FSum r;
	for (double x : xs)
		r += x;
	return double(r);
}

} // namespace util
