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

} // namespace util
