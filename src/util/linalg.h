#pragma once

#include "util/complex.h"
#include <array>
#include <cassert>
#include <initializer_list>
#include <random>
#include <span>

/**
vector/matrix types for fixed (small) dimension. Similar to the GLM library.

Some general notes:
 * The basic type 'T' can be any numerical type which supports arithmetic
   itself. Not just float/double as in GLM.
 * Mixed-Type operations are supported like Vector<double> + Vector<float>
 * The compiler might be able to do some limited auto-vectorization.
   But for serious workloads, you probably need something like 'Vector<simd<T>>'
   using std::experimental::simd, or any available simd-wrapper library.

Notes on 'Vector':
 * Component-wise arithmetic on "Vector" includes operations like
   'vector * vector' and 'vector + scalar' which are nonsense for the
   mathematical concept 'vector', but can be very useful computationally.

Notes on 'Matrix':
 * We store matrices as column-major and consider vectors to be columns.
   So we write the their product as 'Matrix * Vector'. This is the usual
   convention in 3D-Graphics (though OpenGL does not actually require it
   due to user-defined shaders)

Notes on implementation details/performance:
 * You should compile with '-fno-math-errno'. Otherwise you get assmebly like
   if x > 0:
     y = avx_sqrt(x)
   else:
     y = library_sqrt(x).
   Thats a useless branch and a potential slow library call. Just for a
   floating-point-exception that we don't use anyway.
 * The compiler cannot optimize "0+x" to "x", due to signed zeros. So either
   compile with '-fno-signed-zeros' or avoid that code at all by replacing:
     x = 0
     for i = 0 to n:
        x += y[i]
   with
     x = y[0]
     for i = 1 to n:
       x += y[i]
*/

namespace util {

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#pragma clang diagnostic ignored "-Wnested-anon-types"
#elif __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

template <typename T, size_t N> struct VectorStorage
{
	T elements_[N];
	VectorStorage() = default;
	T &operator[](size_t i) { return elements_[i]; }
	T const &operator[](size_t i) const { return elements_[i]; }
};

template <typename T> struct VectorStorage<T, 1>
{
	union
	{
		T elements_[1];
		struct
		{
			T x;
		};
		struct
		{
			T r;
		};
	};
	VectorStorage() = default;
	VectorStorage(T a) : x(std::move(a)) {}
};
template <typename T> struct VectorStorage<T, 2>
{
	union
	{
		T elements_[2];
		struct
		{
			T x, y;
		};
		struct
		{
			T r, g;
		};
	};
	VectorStorage() = default;
	VectorStorage(T a, T b) : x(std::move(a)), y(std::move(b)) {}
};
template <typename T> struct VectorStorage<T, 3>
{
	union
	{
		T elements_[3];
		struct
		{
			T x, y, z;
		};
		struct
		{
			T r, g, b;
		};
	};
	VectorStorage() = default;
	VectorStorage(T a, T b, T c)
	    : x(std::move(a)), y(std::move(b)), z(std::move(c))
	{}
};
template <typename T> struct VectorStorage<T, 4>
{
	union
	{
		T elements_[4];
		struct
		{
			T x, y, z, w;
		};
		struct
		{
			T r, g, b, a;
		};
	};
	VectorStorage() = default;
	VectorStorage(T a, T b, T c, T d)
	    : x(std::move(a)), y(std::move(b)), z(std::move(c)), w(std::move(d))
	{}
};

#ifdef __clang__
#pragma clang diagnostic pop
#elif __GNUC__
#pragma GCC diagnostic pop
#endif

template <typename T, size_t N> struct Vector : VectorStorage<T, N>
{
	using VectorStorage<T, N>::VectorStorage;

	T &operator[](size_t i) { return VectorStorage<T, N>::elements_[i]; }
	T const &operator[](size_t i) const
	{
		return VectorStorage<T, N>::elements_[i];
	}
	T *data() { return VectorStorage<T, N>::elements_; }
	T const *data() const { return VectorStorage<T, N>::elements_; }
	std::span<T> flat() { return std::span(data(), N); }
	std::span<const T> flat() const { return std::span(data(), N); }
};

#define UTIL_DEFINE_VECTOR_OPERATOR(op)                                        \
	template <typename T, typename U, size_t N>                                \
	auto operator op(Vector<T, N> const &a, Vector<U, N> const &b)             \
	    ->Vector<decltype(a[0] op b[0]), N>                                    \
	{                                                                          \
		Vector<decltype(a[0] op b[0]), N> c;                                   \
		for (size_t i = 0; i < N; ++i)                                         \
			c[i] = a[i] op b[i];                                               \
		return c;                                                              \
	}                                                                          \
	template <typename T, typename U, size_t N>                                \
	auto operator op(Vector<T, N> const &a, U const &b)                        \
	    ->Vector<decltype(a[0] op b), N>                                       \
	{                                                                          \
		Vector<decltype(a[0] op b), N> c;                                      \
		for (size_t i = 0; i < N; ++i)                                         \
			c[i] = a[i] op b;                                                  \
		return c;                                                              \
	}                                                                          \
	template <typename T, typename U, size_t N>                                \
	auto operator op(T const &a, Vector<U, N> const &b)                        \
	    ->Vector<decltype(a op b[0]), N>                                       \
	{                                                                          \
		Vector<decltype(a op b[0]), N> c;                                      \
		for (size_t i = 0; i < N; ++i)                                         \
			c[i] = a op b[i];                                                  \
		return c;                                                              \
	}                                                                          \
	template <typename T, typename U, size_t N>                                \
	void operator op##=(Vector<T, N> &a, Vector<U, N> const &b)                \
	{                                                                          \
		for (size_t i = 0; i < N; ++i)                                         \
			a[i] op## = b[i];                                                  \
	}                                                                          \
	template <typename T, typename U, size_t N>                                \
	void operator op##=(Vector<T, N> &a, U const &b)                           \
	{                                                                          \
		for (size_t i = 0; i < N; ++i)                                         \
			a[i] op## = b;                                                     \
	}

UTIL_DEFINE_VECTOR_OPERATOR(+)
UTIL_DEFINE_VECTOR_OPERATOR(-)
UTIL_DEFINE_VECTOR_OPERATOR(*)
UTIL_DEFINE_VECTOR_OPERATOR(/)

#undef UTIL_DEFINE_VECTOR_OPERATOR

template <typename T, typename U, size_t N>
auto dot(Vector<T, N> const &a, Vector<U, N> const &b) -> decltype(a[0] * b[0])
{
	decltype(a[0] * b[0]) r = a[0] * b[0];
	for (size_t i = 1; i < N; ++i)
		r += a[i] * b[i];
	return r;
}
template <typename T, typename U, size_t N>
auto dot(Vector<complex<T>, N> const &a, Vector<U, N> const &b)
    -> decltype(a[0] * b[0])
{
	// This function is somewhat error-prone due to different conventions:
	// GLSL/GLM: no complex numbers at all
	// numpy: 'vdot' does complex conjugation, 'dot' does not
	assert(false && "dot(...) does not support complex numbers. "
	                "Use innerProduct(...) instead.");
}
template <typename T, typename U>
auto cross(Vector<T, 3> const &a, Vector<U, 3> const &b)
    -> Vector<decltype(a[0] * b[0]), 3>
{
	Vector<decltype(a[0] * b[0]), 3> r;
	r[0] = a[1] * b[2] - a[2] * b[1];
	r[1] = a[2] * b[0] - a[0] * b[2];
	r[2] = a[0] * b[1] - a[1] * b[0];
	return r;
}
template <typename T, size_t N> auto norm2(Vector<T, N> const &a)
{
	auto r = norm2(a[0]);
	for (size_t i = 1; i < N; ++i)
		r += norm2(a[i]);
	return r;
}
template <typename T, size_t N> auto length(Vector<T, N> const &a)
{
	return sqrt(norm2(a));
}
template <typename T, size_t N> Vector<T, N> normalize(Vector<T, N> const &a)
{
	// TODO: there should be a slightly faster way to compute 1/sqrt(x)
	return a * (T(1) / length(a));
}
template <typename T, typename U, size_t N>
Vector<T, N> reflect(Vector<T, N> const &a, Vector<U, N> const &normal)
{
	return a - 2 * dot(normal, a) * normal;
}

template <typename T, typename U, size_t N>
auto innerProduct(Vector<T, N> const &a, Vector<U, N> const &b)
    -> decltype(a[0] * b[0])
{
	decltype(a[0] * b[0]) r = conj(a[0]) * b[0];
	for (size_t i = 1; i < N; ++i)
		r += conj(a[i]) * b[i];
	return r;
}

template <typename T, size_t N> struct Matrix
{
	/** only square matrices (yet) */
	Vector<T, N> data_[N];

	Matrix() = default;
	Matrix(T const &a)
	{
		for (size_t i = 0; i < N; ++i)
			for (size_t j = 0; j < N; ++j)
				data_[i][j] = i == j ? a : T(real_t<T>(0));
	}

	static Matrix zero() { return Matrix(T(real_t<T>(0))); }
	static Matrix identity() { return Matrix(T(real_t<T>(1))); }

	T &operator()(size_t i, size_t j)
	{
		assert(i < N && j < N);
		return data_[i][j];
	}
	T const &operator()(size_t i, size_t j) const
	{
		assert(i < N && j < N);
		return data_[i][j];
	}

	Vector<T, N> &operator()(size_t i) { return data_[i]; }
	Vector<T, N> const &operator()(size_t i) const { return data_[i]; }

	T *data() { return data_[0].data(); }
	T const *data() const { return data_[0].data(); }

	std::span<T> flat() { return std::span(data(), N * N); }
	std::span<const T> flat() const { return std::span(data(), N * N); }
};

/** matrix <-> scalar multiplication/division */
template <typename T, typename U, size_t N>
auto operator*(Matrix<T, N> const &a, U const &b)
    -> Matrix<decltype(a(0, 0) * b), N>
{
	Matrix<decltype(a(0, 0) * b), N> r;
	for (size_t i = 0; i < N * N; ++i)
		r.data()[i] = a.data()[i] * b;
	return r;
}
template <typename T, typename U, size_t N>
auto operator/(Matrix<T, N> const &a, U const &b)
    -> Matrix<decltype(a(0, 0) / b), N>
{
	Matrix<decltype(a(0, 0) / b), N> r;
	for (size_t i = 0; i < N * N; ++i)
		r.data()[i] = a.data()[i] / b;
	return r;
}

/** matrix <-> vector multiplication */
template <typename T, typename U, size_t N>
auto operator*(Matrix<T, N> const &a, Vector<U, N> const &b)
    -> Vector<decltype(a(0, 0) * b[0]), N>
{
	Vector<decltype(a(0, 0) * b[0]), N> r;
	for (size_t i = 0; i < N; ++i)
	{
		r[i] = T(0);
		for (size_t j = 0; j < N; ++j)
			r[i] += a(i, j) * b[j];
	}
	return r;
}

template <typename T, typename U, size_t N>
auto operator+(Matrix<T, N> const &a, Matrix<U, N> const &b)
    -> Matrix<decltype(a(0, 0) + b(0, 0)), N>
{
	Matrix<decltype(a(0, 0) + b(0, 0)), N> r;
	for (size_t i = 0; i < N; ++i)
		for (size_t j = 0; j < N; ++j)
			r(i, j) = a(i, j) + b(i, j);
	return r;
}

template <typename T, typename U, size_t N>
auto operator-(Matrix<T, N> const &a, Matrix<U, N> const &b)
    -> Matrix<decltype(a(0, 0) - b(0, 0)), N>
{
	Matrix<decltype(a(0, 0) - b(0, 0)), N> r;
	for (size_t i = 0; i < N; ++i)
		for (size_t j = 0; j < N; ++j)
			r(i, j) = a(i, j) - b(i, j);
	return r;
}

/** matrix <-> matrix multiplication */
template <typename T, typename U, size_t N>
auto operator*(Matrix<T, N> const &a, Matrix<U, N> const &b)
    -> Matrix<decltype(a(0, 0) * b(0, 0)), N>
{
	Matrix<decltype(a(0, 0) * b(0, 0)), N> r;
	for (size_t i = 0; i < N; ++i)
		for (size_t j = 0; j < N; ++j)
		{
			r(i, j) = a(i, 0) * b(0, j);
			for (size_t k = 1; k < N; ++k)
				r(i, j) += a(i, k) * b(k, j);
		}
	return r;
}

/** inplace operations */
template <typename T, typename U, size_t N>
Matrix<T, N> &operator+=(Matrix<T, N> &a, Matrix<U, N> const &b)
{
	for (size_t i = 0; i < N; ++i)
		for (size_t j = 0; j < N; ++j)
			a(i, j) += b(i, j);
	return a;
}
template <typename T, typename U, size_t N>
Matrix<T, N> &operator-=(Matrix<T, N> &a, Matrix<U, N> const &b)
{
	for (size_t i = 0; i < N; ++i)
		for (size_t j = 0; j < N; ++j)
			a(i, j) -= b(i, j);
	return a;
}

template <typename T, size_t N> Matrix<T, N> transpose(Matrix<T, N> const &a)
{
	Matrix<T, N> r;
	for (size_t i = 0; i < N; ++i)
		for (size_t j = 0; j < N; ++j)
			r(i, j) = a(j, i);
	return r;
}

template <typename T, size_t N> Matrix<T, N> conj(Matrix<T, N> const &a)
{
	Matrix<T, N> r;
	for (size_t i = 0; i < N; ++i)
		for (size_t j = 0; j < N; ++j)
			r(i, j) = conj(a(i, j));
	return r;
}

template <typename T, size_t N> Matrix<T, N> adj(Matrix<T, N> const &a)
{
	Matrix<T, N> r;
	for (size_t i = 0; i < N; ++i)
		for (size_t j = 0; j < N; ++j)
			r(i, j) = conj(a(j, i));
	return r;
}

template <typename T, size_t N>
Matrix<T, N> antiHermitianTraceless(Matrix<T, N> const &a)
{
	// TODO: optimize
	auto r = (a - adj(a)) / real_t<T>(2);
	return r - Matrix<T, N>{trace(r) / real_t<T>(N)};
}

template <typename T, size_t N> T trace(Matrix<T, N> const &a)
{
	T r = a(0, 0);
	for (size_t i = 1; i < N; ++i)
		r += a(i, i);
	return r;
}

template <typename T> T determinant(Matrix<T, 2> const &a)
{
	return a(0, 0) * a(1, 1) - a(0, 1) * a(1, 0);
}

template <typename T> T determinant(Matrix<T, 3> const &a)
{
	return a(0, 0) * (a(1, 1) * a(2, 2) - a(2, 1) * a(1, 2)) -
	       a(0, 1) * (a(1, 0) * a(2, 2) - a(1, 2) * a(2, 0)) +
	       a(0, 2) * (a(1, 0) * a(2, 1) - a(1, 1) * a(2, 0));
}

template <typename T> Matrix<T, 3> inverse(Matrix<T, 3> const &a)
{
	Matrix<T, 3> b;
	b(0, 0) = a(1, 1) * a(2, 2) - a(2, 1) * a(1, 2);
	b(0, 1) = a(0, 2) * a(2, 1) - a(0, 1) * a(2, 2);
	b(0, 2) = a(0, 1) * a(1, 2) - a(0, 2) * a(1, 1);
	b(1, 0) = a(1, 2) * a(2, 0) - a(1, 0) * a(2, 2);
	b(1, 1) = a(0, 0) * a(2, 2) - a(0, 2) * a(2, 0);
	b(1, 2) = a(1, 0) * a(0, 2) - a(0, 0) * a(1, 2);
	b(2, 0) = a(1, 0) * a(2, 1) - a(2, 0) * a(1, 1);
	b(2, 1) = a(2, 0) * a(0, 1) - a(0, 0) * a(2, 1);
	b(2, 2) = a(0, 0) * a(1, 1) - a(1, 0) * a(0, 1);

	return b * (T(1) / determinant(a));
}

template <typename T, size_t N> auto norm2(Matrix<T, N> const &a)
{
	auto r = norm2(a(0));
	for (size_t i = 1; i < N; ++i)
		r += norm2(a(i));
	return r;
}

template <typename T, size_t N> Matrix<T, N> gramSchmidt(Matrix<T, N> a)
{
	for (size_t i = 0; i < N; ++i)
	{
		for (size_t j = 0; j < i; ++j)
			a(i) -= a(j) * innerProduct(a(j), a(i));
		a(i) = normalize(a(i));
	}
	return a;
}

template <typename T, size_t N> Matrix<T, N> exp(Matrix<T, N> const &a)
{
	// use exp(A) = exp(A/16)^16 with 12th-order Taylor
	// TODO (ideas):
	//    * handle the trace of a separately (and exactly)
	//    * choose the expansion order depending on the norm of a
	//    * use exact formulas for small N
	auto b = a * (1.0 / 16.0);
	auto r = Matrix<T, N>::identity() + b;
	for (int n = 2; n <= 12; ++n)
	{
		b = a * b * (1.0 / (16.0 * n));
		r += b;
	}
	for (int i = 0; i < 4; ++i)
		r = r * r;
	return r;
}

// overloads for (horizontal) simd

template <typename T, size_t N> auto vsum(Vector<T, N> const &a)
{
	Vector<decltype(vsum(a[0])), N> r;
	for (size_t i = 0; i < N; ++i)
		r[i] = vsum(a[i]);
	return r;
}
template <typename T, size_t N>
auto vextract(Vector<T, N> const &a, size_t lane)
{
	Vector<decltype(vsum(a[0])), N> r;
	for (size_t i = 0; i < N; ++i)
		r[i] = vextract(a[i], lane);
	return r;
}
template <typename T, typename U, size_t N>
void vinsert(Vector<T, N> &a, size_t lane, Vector<U, N> const &b)
{
	for (size_t i = 0; i < N; ++i)
		vinsert(a[i], lane, b[i]);
}

template <typename T, size_t N> auto vsum(Matrix<T, N> const &a)
{
	Matrix<decltype(vsum(a(0, 0))), N> r;
	for (size_t i = 0; i < N; ++i)
		for (size_t j = 0; j < N; ++j)
			r(i, j) = vsum(a(i, j));
	return r;
}
template <typename T, size_t N>
auto vextract(Matrix<T, N> const &a, size_t lane)
{
	Matrix<decltype(vsum(a(0, 0))), N> r;
	for (size_t i = 0; i < N; ++i)
		for (size_t j = 0; j < N; ++j)
			r(i, j) = vextract(a(i, j), lane);
	return r;
}
template <typename T, typename U, size_t N>
void vinsert(Matrix<T, N> &a, size_t lane, Matrix<U, N> const &b)
{
	for (size_t i = 0; i < N; ++i)
		for (size_t j = 0; j < N; ++j)
			vinsert(a(i, j), lane, b(i, j));
}

/**
 * Random point on a sphere with uniform distribution.
 */
template <typename T> class uniform_sphere_distribution;

template <typename RealType, size_t N>
class uniform_sphere_distribution<Vector<RealType, N>>
{
	// internal helpers
	std::normal_distribution<RealType> normal;

  public:
	/** types */
	using result_type = Vector<RealType, N>;

	/** constructors */
	uniform_sphere_distribution() : normal(0.0, 1.0) {}

	/** generate the next random value */
	template <class Generator> result_type operator()(Generator &rng)
	{
		result_type r;
		for (size_t i = 0; i < N; ++i)
			r[i] = normal(rng);
		return normalize(r);
	}
};

template <typename T> class exponential_sphere_distribution;

/**
 * Random point on a sphere with distribution P(r) = e^(-alpha r[0]).
 */
template <typename RealType>
class exponential_sphere_distribution<Vector<RealType, 3>>
{
	RealType alpha_;
	std::normal_distribution<RealType> normal;
	std::uniform_real_distribution<double> uniform;

  public:
	/** types */
	using result_type = Vector<RealType, 3>;

	/** constructors */
	exponential_sphere_distribution(RealType alpha)
	    : alpha_(alpha), normal(0.0, 1.0), uniform(exp(-2 * alpha), 1.0)
	{
		assert(alpha > 0.0);
	}

	/** generate the next random value */
	template <class Generator> result_type operator()(Generator &rng)
	{
		result_type r;

		// NOTE: In 3D, each individual component of a uniform
		//       sphere-distribution has an exact uniform [-1,1]-distribution.
		//       For the exponential sphere-distribution, the first component
		//       can therefore simply be generated as a 1D (truncated)
		//       exponential distribution. This is only true in 3D (!).

		// exponential distribution of r[0]
		r[0] = log(uniform(rng)) / alpha_ + 1;

		// uniform sphere distribution of remaining entries
		r[1] = normal(rng);
		r[2] = normal(rng);
		double s = sqrt((1 - r[0] * r[0]) / (r[1] * r[1] + r[2] * r[2]));
		r[1] *= s;
		r[2] *= s;
		return r;
	}
};

} // namespace util

namespace fmt {
template <typename T, size_t N>
struct formatter<util::Matrix<T, N>> : formatter<T>
{
	// NOTE: parse() is inherited from formatter<T>

	template <typename FormatContext>
	auto format(const util::Matrix<T, N> &a, FormatContext &ctx)
	    -> decltype(ctx.out())
	{
		format_to(ctx.out(), "[[");
		for (size_t i = 0; i < N; ++i)
		{
			for (size_t j = 0; j < N; ++j)
			{
				if (j != 0)
					format_to(ctx.out(), ", ");
				formatter<T>::format(a(i, j), ctx);
			}

			if (i + 1 == N)
				format_to(ctx.out(), "]]");
			else
				format_to(ctx.out(), "],\n [");
		}
		return ctx.out();
	}
};
} // namespace fmt
