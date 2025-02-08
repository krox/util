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

template <class R, int N> bool operator==(Series<R, N> const &a, R const &b)
{
	for (int i = 1; i < N; ++i)
		if (!is_zero(a[i]))
			return false;
	return a[0] == b;
}

template <typename R, int N> bool operator==(Series<R, N> const &a, int b)
{
	for (int i = 1; i <= N; ++i)
		if (!is_zero(a[i]))
			return false;
	return a[0] == b;
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
auto operator*(Series<R, N> const &a, Series<S, N> const &b)
{
	using T = decltype(a[0] * b[0]);
	auto r = Series<T, N>(0);
	for (int i = 0; i < N; ++i)
		for (int j = 0; i + j < N; ++j)
			r[i + j] += a[i] * b[j];
	return r;
}

template <class R, int N> bool is_zero(Series<R, N> const &s)
{
	for (R const &c : s.coefficients())
		if (!is_zero(c))
			return false;
	return true;
}

} // namespace util

template <class R, int N> struct fmt::formatter<util::Series<R, N>>
{
	constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

	auto format(const util::Series<R, N> &s,
	            auto &ctx) const -> decltype(ctx.out())
	{
		using util::is_negative;
		using util::is_zero;

		auto it = ctx.out();

		bool first = true;
		bool printed_someting = false;
		for (int i = 0; i < N; ++i)
		{
			auto c = s[i];
			if (is_zero(c))
				continue;
			printed_someting = true;

			if (is_negative(c))
			{
				if (first)
					it = fmt::format_to(it, "-");
				else
					it = fmt::format_to(it, " - ");
				c = -c;
			}
			else if (!first)
				it = fmt::format_to(it, " + ");
			first = false;

			if (i == 0)
				it = fmt::format_to(it, "({})", c);
			else if (!(c == 1))
				it = fmt::format_to(it, "({})*", c);

			if (i == 1)
				it = fmt::format_to(it, "{}", "ε");
			else if (i >= 2)
				it = fmt::format_to(it, "{}^{}", "ε", i);
		}

		if (!printed_someting)
			it = fmt::format_to(it, "0");

		it = fmt::format_to(it, " + O({}^{})", "ε", N);

		return it;
	}
};
