#pragma once

#include "fmt/format.h"
#include "util/linalg.h"
#include <array>
#include <cassert>
#include <climits>
#include <utility>
#include <vector>

namespace util {

// Truncated series with compile-time order.
template <class R, int N> class Series
{
	static_assert(N >= 2); // I guess N=1 would be allowable too?

  public:
	// The coeffs dont fulfill any invariants, so they can be public.
	std::array<R, N> coefficients_;

  public:
	Series() = default;

	explicit Series(int c)
	{
		coefficients_.fill(R(0));
		coefficients_[0] = R(c);
	}

	explicit Series(R c)
	{
		coefficients_.fill(R(0));
		coefficients_[0] = std::move(c);
	}

	static Series generator()
	{
		assert(N >= 2);
		Series r;
		r.coefficients_.fill(R(0));
		r.coefficients_[1] = R(1);
		return r;
	}

	auto &coefficients() { return coefficients_; }
	auto const &coefficients() const { return coefficients_; }

	// array-like access to coefficients
	constexpr size_t size() const noexcept { return N; }
	constexpr R *data() noexcept { return coefficients_.data(); }
	constexpr R const *data() const noexcept { return coefficients_.data(); }
	R &operator[](int i) noexcept { return coefficients_.at(i); }
	R const &operator[](int i) const noexcept { return coefficients_.at(i); }
};

template <class R, class U, int N>
bool operator==(Series<R, N> const &a, Series<U, N> const &b)
{
	for (int i = 0; i < N; ++i)
		if (a[i] != b[i])
			return false;
	return true;
}

// element-wise and scalar operations. Same as for the chalk::Vector class
UTIL_UNARY_OP(Series, N, -)
UTIL_ELEMENT_WISE_OP(Series, N, +)
UTIL_ELEMENT_WISE_OP(Series, N, -)
UTIL_SCALAR_OP(Series, std::type_identity_t<T>, N, *)
UTIL_SCALAR_OP(Series, std::type_identity_t<T>, N, /)
UTIL_SCALAR_OP_LEFT(Series, std::type_identity_t<T>, N, *)
UTIL_SCALAR_OP(Series, T::value_type, N, *)
UTIL_SCALAR_OP(Series, T::value_type, N, /)
UTIL_SCALAR_OP_LEFT(Series, T::value_type, N, *)

// series <-> series multiplication
template <class R, class S, int N>
auto operator*(Series<R, N> const &a,
               Series<S, N> const &b) -> Series<decltype(a[0] * b[0]), N>
{
	using T = decltype(a[0] * b[0]);
	Series<T, N> r;
	for (int j = 0; j < N; ++j)
		r[j] = a[0] * b[j];
	for (int i = 1; i < N; ++i)
		for (int j = 0; i + j < N; ++j)
			r[i + j] += a[i] * b[j];
	return r;
}

} // namespace util

namespace fmt {
template <class T, int N> struct formatter<util::Series<T, N>> : formatter<T>
{
	auto format(util::Series<T, N> const &a,
	            auto &ctx) const -> decltype(ctx.out())
	{
		format_to(ctx.out(), "[");
		for (int i = 0; i < N; ++i)
		{
			if (i != 0)
				format_to(ctx.out(), ", ");
			formatter<T>::format(a[i], ctx);
		}
		format_to(ctx.out(), "]");
		return ctx.out();
	}
};
} // namespace fmt
