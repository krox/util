#pragma once

#include "util/complex.h"
#include <array>
#include <cassert>
#include <initializer_list>
#include <random>
#include <span>

/**
vector/matrix types for fixed (small) dimension. Similar to the GLM library.

Should be useable for both computer graphics (e.g., 3D rendering) and also some
scientific workloads (e.g., lattice gauge theory).

Some general notes:
 * The basic type 'T' can be any numerical type which supports arithmetic
   itself, not just float/double as in GLM. For example:
       * complex numbers: complex<...>, quaternion<...>
       * vectorization: simd<...>
       * high-precision: ddouble, MPFRFloat
 * Some mixed-type operations are supported like float<->double or
   simd<->scalar.
 * The compiler might be able to do some limited auto-vectorization. But for
   serious workloads, the recommended way to go is "horizontal" vectorization
   using something like
       util::Vector<util::simd<...>>
   to process multiple Vector's in parallel

Notes on 'Vector':
 * Component-wise arithmetic on "Vector" includes operations like
   'vector * vector' and 'vector + scalar' which are nonsense for the
   mathematical concept 'vector', but can be very useful computationally.

Notes on implementation details/performance:
 * You should compile with '-fno-math-errno'. Otherwise you get assmebly like
   if x > 0:
     y = avx_sqrt(x)
   else:
     y = library_sqrt(x).
   Thats a useless branch and a potential slow library call. Just for a
   floating-point-exception that nobody uses anyway.
 * The compiler cannot optimize "0+x" to "x", due to signed zeros. So either
   compile with '-fno-signed-zeros' or avoid that code altogether by replacing:
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

template <class T, int N> struct VectorStorage
{
	T elements_[N];
	VectorStorage() = default;
	T &operator[](size_t i) noexcept { return elements_[i]; }
	T const &operator[](size_t i) const noexcept { return elements_[i]; }
};

template <class T> struct VectorStorage<T, 1>
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
	VectorStorage(T a) noexcept : x(std::move(a)) {}
};
template <class T> struct VectorStorage<T, 2>
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
	VectorStorage(T a, T b) noexcept : x(std::move(a)), y(std::move(b)) {}
};
template <class T> struct VectorStorage<T, 3>
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
	VectorStorage(T a, T b, T c) noexcept
	    : x(std::move(a)), y(std::move(b)), z(std::move(c))
	{}
};
template <class T> struct VectorStorage<T, 4>
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
	VectorStorage(T a, T b, T c, T d) noexcept
	    : x(std::move(a)), y(std::move(b)), z(std::move(c)), w(std::move(d))
	{}
};

#ifdef __clang__
#pragma clang diagnostic pop
#elif __GNUC__
#pragma GCC diagnostic pop
#endif

template <class T, int N> struct Vector : VectorStorage<T, N>
{
	using value_type = T;
	static constexpr size_t size() noexcept { return N; }
	static constexpr int dim() noexcept { return N; }

	using VectorStorage<T, N>::VectorStorage;

	// create vector with i.i.d. gaussian entries
	template <class Rng> static Vector random_normal(Rng &rng) noexcept
	{
		Vector r;
		for (int i = 0; i < N; ++i)
			r.data()[i] = rng.template normal<T>();
		return r;
	}

	UTIL_DEVICE T &operator[](size_t i) noexcept
	{
		return VectorStorage<T, N>::elements_[i];
	}
	UTIL_DEVICE T const &operator[](size_t i) const noexcept
	{
		return VectorStorage<T, N>::elements_[i];
	}

	UTIL_DEVICE T &operator()(int i) noexcept
	{
		return VectorStorage<T, N>::elements_[i];
	}
	UTIL_DEVICE T const &operator()(int i) const noexcept
	{
		return VectorStorage<T, N>::elements_[i];
	}

	UTIL_DEVICE T *data() noexcept { return VectorStorage<T, N>::elements_; }
	UTIL_DEVICE T const *data() const noexcept
	{
		return VectorStorage<T, N>::elements_;
	}
	std::span<T> flat() noexcept { return std::span(data(), N); }
	std::span<const T> flat() const noexcept { return std::span(data(), N); }
};

// Helper macros to define the plethora of vector-vector and vector-scalar
// operators. Might seem a lot of individual definitions, but the alternatives
// have serious downsides too:
//   * Allow implicit casts like real->complex or scalar->vector:
//     Might have performance impact if the compiler cant reason through
//     redundant instructions. Especially true with strict FP rules (x+0==x is
//     not always true in IEEE) or types calling external libraries (mpfr,
//     libquadmath, ...)
//   * catch-all templates for scalar multiplications like
//           template<T,U> operator*(vector<T>, U)
//     will lead to ambiguities in complicated (deeply nested) cases

#define UTIL_UNARY_OP(V, DIM, op)                                              \
	template <class T, int N>                                                  \
	UTIL_DEVICE auto operator op(V<T, N> const &a) noexcept                    \
	{                                                                          \
		V<decltype(op a.data()[0]), N> r;                                      \
		for (int i = 0; i < DIM; ++i)                                          \
			r.data()[i] = op a.data()[i];                                      \
		return r;                                                              \
	}

#define UTIL_ELEMENT_WISE_OP(V, DIM, op)                                       \
	template <class T, class U, int N>                                         \
	UTIL_DEVICE auto operator op(V<T, N> const &a, V<U, N> const &b) noexcept  \
	{                                                                          \
		V<decltype(a.data()[0] op b.data()[0]), N> r;                          \
		for (int i = 0; i < DIM; ++i)                                          \
			r.data()[i] = a.data()[i] op b.data()[i];                          \
		return r;                                                              \
	}                                                                          \
	template <class T, class U, int N>                                         \
	UTIL_DEVICE V<T, N> &operator op##=(V<T, N> &a, V<U, N> const &b) noexcept \
	{                                                                          \
		for (int i = 0; i < DIM; ++i)                                          \
			a.data()[i] op## = b.data()[i];                                    \
		return a;                                                              \
	}

#define UTIL_SCALAR_OP(V, S, DIM, op)                                          \
	template <class T, int N>                                                  \
	UTIL_DEVICE auto operator op(V<T, N> const &a, typename S b) noexcept      \
	{                                                                          \
		V<T, N> r;                                                             \
		for (int i = 0; i < DIM; ++i)                                          \
			r.data()[i] = a.data()[i] op b;                                    \
		return r;                                                              \
	}                                                                          \
	template <class T, int N>                                                  \
	UTIL_DEVICE V<T, N> &operator op##=(V<T, N> &a, typename S b) noexcept     \
	{                                                                          \
		for (int i = 0; i < DIM; ++i)                                          \
			a.data()[i] op## = b;                                              \
		return a;                                                              \
	}

#define UTIL_SCALAR_OP_LEFT(V, S, DIM, op)                                     \
	template <class T, int N>                                                  \
	UTIL_DEVICE auto operator op(typename S a, V<T, N> const &b) noexcept      \
	{                                                                          \
		V<T, N> r;                                                             \
		for (int i = 0; i < DIM; ++i)                                          \
			r.data()[i] = a op b.data()[i];                                    \
		return r;                                                              \
	}

UTIL_UNARY_OP(Vector, N, -)

UTIL_ELEMENT_WISE_OP(Vector, N, +)
UTIL_ELEMENT_WISE_OP(Vector, N, -)
UTIL_ELEMENT_WISE_OP(Vector, N, *)
UTIL_ELEMENT_WISE_OP(Vector, N, /)

// the 'type_identity_t' disables template type inference, so that things like
//     Vector<float> * double
// get resolved with T=float instead of giving ambiguity errors
UTIL_SCALAR_OP(Vector, std::type_identity_t<T>, N, +)
UTIL_SCALAR_OP(Vector, std::type_identity_t<T>, N, -)
UTIL_SCALAR_OP(Vector, std::type_identity_t<T>, N, *)
UTIL_SCALAR_OP(Vector, std::type_identity_t<T>, N, /)
UTIL_SCALAR_OP_LEFT(Vector, std::type_identity_t<T>, N, +)
UTIL_SCALAR_OP_LEFT(Vector, std::type_identity_t<T>, N, -)
UTIL_SCALAR_OP_LEFT(Vector, std::type_identity_t<T>, N, *)

// These overloads handle one level of nesting for cases like
// T in { Vector<...>, Matrix<...>, Simd<...>, Complex<...> }
// could add more levels in the future if necessary. Thanks to SFINAE, nothing
// happens if T is something simpler, i.e. T=double
UTIL_SCALAR_OP(Vector, T::value_type, N, +)
UTIL_SCALAR_OP(Vector, T::value_type, N, -)
UTIL_SCALAR_OP(Vector, T::value_type, N, *)
UTIL_SCALAR_OP(Vector, T::value_type, N, /)
UTIL_SCALAR_OP_LEFT(Vector, T::value_type, N, +)
UTIL_SCALAR_OP_LEFT(Vector, T::value_type, N, -)
UTIL_SCALAR_OP_LEFT(Vector, T::value_type, N, *)

// dot product of two vectors. No complex conjucation.
template <class T, class U, int N>
UTIL_DEVICE auto dot(Vector<T, N> const &a, Vector<U, N> const &b) noexcept
    -> decltype(a(0) * b(0))
{
	decltype(a(0) * b(0)) r = a(0) * b(0);
	for (int i = 1; i < N; ++i)
		r += a(i) * b(i);
	return r;
}

template <class T, class U, int N>
UTIL_DEVICE auto dot(Vector<complex<T>, N> const &a,
                     Vector<U, N> const &b) noexcept -> decltype(a(0) * b(0))
{
	// This function is somewhat error-prone due to different conventions:
	// GLSL/GLM: no complex numbers at all
	// numpy: 'dot' does not do complex conjugation (but 'vdot' does)
	// Eigen: 'dot' does complex conjugation
	assert(false && "dot(...) does not support complex numbers. "
	                "Use inner_product(...) instead.");
}

// same as 'dot(...)', but with complex conjugation of the left argument
template <class T, class U, int N>
UTIL_DEVICE auto inner_product(Vector<T, N> const &a,
                               Vector<U, N> const &b) noexcept
    -> decltype(conj(a(0)) * b(0))
{
	auto r = conj(a(0)) * b(0);
	for (int i = 1; i < N; ++i)
		r += conj(a(i)) * b(i);
	return r;
}

// 3-dimensional cross product. (no complex conjugation).
template <class T, class U>
UTIL_DEVICE auto cross(Vector<T, 3> const &a, Vector<U, 3> const &b) noexcept
    -> Vector<decltype(a(0) * b(0)), 3>
{
	Vector<decltype(a(0) * b(0)), 3> r;
	r(0) = a(1) * b(2) - a(2) * b(1);
	r(1) = a(2) * b(0) - a(0) * b(2);
	r(2) = a(0) * b(1) - a(1) * b(0);
	return r;
}

// squared L^2 norm.
// In simple cases this is equivalent to 'inner_product(a,a)' but:
//     * potentially more efficient: return type is real, even for complex T
//     * calls 'norm2(...)' recursively, which means some nested types are
//       collapsed, depending on the type T. For example:
//           norm2(Vector<Matrix<complex<simd<double>>>>) -> simd<double>
template <class T, int N> UTIL_DEVICE auto norm2(Vector<T, N> const &a) noexcept
{
	auto r = norm2(a(0));
	for (int i = 1; i < N; ++i)
		r += norm2(a(i));
	return r;
}

// (non-squared) L^2 norm
template <class T, int N>
UTIL_DEVICE auto length(Vector<T, N> const &a) noexcept
{
	return sqrt(norm2(a));
}

// same as 'a / length(a)'
template <class T, int N>
UTIL_DEVICE Vector<T, N> normalize(Vector<T, N> const &a) noexcept
{
	// TODO: there should be a slightly faster way to compute 1/sqrt(x)
	return a * (T(1) / length(a));
}

// geometric reflcetion of a along the normal vector n
//     * n must be normalized already
template <class T, class U, int N>
UTIL_DEVICE Vector<T, N> reflect(Vector<T, N> const &a,
                                 Vector<U, N> const &n) noexcept
{
	return a - 2 * dot(n, a) * n;
}

template <class T, int N> struct Matrix
{
	using value_type = T;
	static constexpr int dim() noexcept { return N; }

	using Vector = util::Vector<T, N>;

	Vector data_[N];

	Matrix() = default;

	UTIL_DEVICE Matrix(T const &a) noexcept
	{
		for (int i = 0; i < N; ++i)
			for (int j = 0; j < N; ++j)
				(*this)(i, j) = i == j ? a : T(0);
	}

	template <std::convertible_to<typename T::value_type> V>
	UTIL_DEVICE Matrix(V const &a) noexcept
	{
		// NOTE: the extra template-indirection with 'V' is necessary in case
		// 'T' is not a class type (i.e. T=float/double), which makes
		// 'T::value_type' invalid. This way, its SFINAE'd away.
		for (int i = 0; i < N; ++i)
			for (int j = 0; j < N; ++j)
				(*this)(i, j) = i == j ? T(a) : T(typename T::value_type(0));
	}

	static UTIL_DEVICE Matrix zero() noexcept { return Matrix(T(0)); }
	static UTIL_DEVICE Matrix identity() noexcept { return Matrix(T(1)); }

	// create matrix with i.i.d. gaussian entries
	template <class Rng> static Matrix random_normal(Rng &rng) noexcept
	{
		Matrix r;
		for (int i = 0; i < N * N; ++i)
			r.data()[i] = rng.template normal<T>();
		return r;
	}

	UTIL_DEVICE T &operator()(int i, int j) noexcept
	{
		// row major order
		assert(0 <= i && i < N && 0 <= j && j < N);
		return data_[i][j];
	}
	UTIL_DEVICE T const &operator()(int i, int j) const noexcept
	{
		assert(0 <= i && i < N && 0 <= j && j < N);
		return data_[i][j];
	}

	UTIL_DEVICE Vector &operator()(int i) noexcept
	{
		assert(0 <= i && i < N);
		return data_[i];
	}
	UTIL_DEVICE Vector const &operator()(int i) const noexcept
	{
		assert(0 <= i && i < N);
		return data_[i];
	}

	UTIL_DEVICE T *data() noexcept { return data_[0].data(); }
	UTIL_DEVICE T const *data() const noexcept { return data_[0].data(); }

	std::span<T> flat() noexcept { return std::span(data(), N * N); }
	std::span<const T> flat() const noexcept
	{
		return std::span(data(), N * N);
	}
};

UTIL_UNARY_OP(Matrix, N *N, -)
UTIL_ELEMENT_WISE_OP(Matrix, N *N, +)
UTIL_ELEMENT_WISE_OP(Matrix, N *N, -)
UTIL_SCALAR_OP(Matrix, std::type_identity_t<T>, N *N, *)
UTIL_SCALAR_OP(Matrix, std::type_identity_t<T>, N *N, /)
UTIL_SCALAR_OP_LEFT(Matrix, std::type_identity_t<T>, N *N, *)
UTIL_SCALAR_OP(Matrix, T::value_type, N *N, *)
UTIL_SCALAR_OP(Matrix, T::value_type, N *N, /)
UTIL_SCALAR_OP_LEFT(Matrix, T::value_type, N *N, *)

// matrix-vector multiplication
template <class T, class U, int N>
UTIL_DEVICE auto operator*(Matrix<T, N> const &a,
                           Vector<U, N> const &b) noexcept
    -> Vector<decltype(a(0, 0) * b(0)), N>
{
	Vector<decltype(a(0, 0) * b(0)), N> r;
	for (int i = 0; i < N; ++i)
	{
		r(i) = a(i, 0) * b(0);
		for (int j = 1; j < N; ++j)
			r(i) += a(i, j) * b[j];
	}
	return r;
}

// matrix-matrix multiplication
template <class T, class U, int N>
UTIL_DEVICE auto operator*(Matrix<T, N> const &a,
                           Matrix<U, N> const &b) noexcept
    -> Matrix<decltype(a(0, 0) * b(0, 0)), N>
{
	Matrix<decltype(a(0, 0) * b(0, 0)), N> r;
	for (int i = 0; i < N; ++i)
		for (int j = 0; j < N; ++j)
		{
			r(i, j) = a(i, 0) * b(0, j);
			for (int k = 1; k < N; ++k)
				r(i, j) += a(i, k) * b(k, j);
		}
	return r;
}

template <class T, class U, int N>
UTIL_DEVICE Matrix<T, N> &operator*=(Matrix<T, N> &a,
                                     Matrix<U, N> const &b) noexcept
{
	a = a * b;
	return a;
}

template <class T, int N>
UTIL_DEVICE Matrix<T, N> transpose(Matrix<T, N> const &a) noexcept
{
	Matrix<T, N> r;
	for (int i = 0; i < N; ++i)
		for (int j = 0; j < N; ++j)
			r(i, j) = a(j, i);
	return r;
}

// element-wise complex conjugation
template <class T, int N>
UTIL_DEVICE Matrix<T, N> conj(Matrix<T, N> const &a) noexcept
{
	Matrix<T, N> r;
	for (int i = 0; i < N; ++i)
		for (int j = 0; j < N; ++j)
			r(i, j) = conj(a(i, j));
	return r;
}

// adjoint matrix, i.e., transpose and complex conjugate
template <class T, int N>
UTIL_DEVICE Matrix<T, N> adj(Matrix<T, N> const &a) noexcept
{
	Matrix<T, N> r;
	for (int i = 0; i < N; ++i)
		for (int j = 0; j < N; ++j)
			r(i, j) = conj(a(j, i));
	return r;
}

template <class T, int N>
Matrix<util::complex<T>, N> UTIL_DEVICE
hermitian_traceless(Matrix<util::complex<T>, N> const &a) noexcept
{
	Matrix<util::complex<T>, N> r;
	for (int i = 0; i < N; ++i)
		for (int j = i + 1; j < N; ++j)
		{
			r(i, j) = (a(i, j) + conj(a(j, i))) * 0.5;
			r(j, i) = conj(r(i, j));
		}

	T t = 0;
	for (int i = 0; i < N; ++i)
		t += a(i, i).re;
	for (int i = 0; i < N; ++i)
	{
		r(i, i).re = a(i, i).re - t / T(N);
		r(i, i).im = T(0);
	}

	return r;
}

template <class T, int N>
Matrix<util::complex<T>, N> UTIL_DEVICE
antihermitian_traceless(Matrix<util::complex<T>, N> const &a) noexcept
{
	Matrix<util::complex<T>, N> r;
	for (int i = 0; i < N; ++i)
		for (int j = i + 1; j < N; ++j)
		{
			r(i, j) = (a(i, j) - conj(a(j, i))) * 0.5;
			r(j, i) = -conj(r(i, j));
		}

	T t = 0;
	for (int i = 0; i < N; ++i)
		t += a(i, i).im;
	for (int i = 0; i < N; ++i)
	{
		r(i, i).re = T(0);
		r(i, i).im = a(i, i).im - t / T(N);
	}

	return r;
}

template <class T, int N> UTIL_DEVICE T trace(Matrix<T, N> const &a) noexcept
{
	T r = a(0, 0);
	for (int i = 1; i < N; ++i)
		r += a(i, i);
	return r;
}

template <class T> UTIL_DEVICE T determinant(Matrix<T, 2> const &a) noexcept
{
	return a(0, 0) * a(1, 1) - a(0, 1) * a(1, 0);
}

template <class T> UTIL_DEVICE T determinant(Matrix<T, 3> const &a) noexcept
{
	return a(0, 0) * (a(1, 1) * a(2, 2) - a(2, 1) * a(1, 2)) -
	       a(0, 1) * (a(1, 0) * a(2, 2) - a(1, 2) * a(2, 0)) +
	       a(0, 2) * (a(1, 0) * a(2, 1) - a(1, 1) * a(2, 0));
}

template <class T>
UTIL_DEVICE Matrix<T, 2> inverse(Matrix<T, 2> const &a) noexcept
{
	Matrix<T, 2> b;
	b(0, 0) = a(1, 1);
	b(0, 1) = -a(0, 1);
	b(1, 0) = -a(1, 0);
	b(1, 1) = a(0, 0);
	return b * (T(1) / determinant(a));
}

template <class T>
UTIL_DEVICE Matrix<T, 3> inverse(Matrix<T, 3> const &a) noexcept
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

template <class T, int N> UTIL_DEVICE auto norm2(Matrix<T, N> const &a) noexcept
{
	auto r = norm2(a.data()[0]);
	for (int i = 1; i < N * N; ++i)
		r += norm2(a.data()[i]);
	return r;
}

// orthonormalize the rows of a using Gram-Schmidt method
template <class T, int N>
UTIL_DEVICE Matrix<T, N> gram_schmidt(Matrix<T, N> a) noexcept
{
	for (int i = 0; i < N; ++i)
	{
		for (int j = 0; j < i; ++j)
			a(i) -= a(j) * inner_product(a(j), a(i));
		a(i) = normalize(a(i));
	}
	return a;
}

// matrix exponential using fixed-order Taylor approximation
template <class T, int N>
UTIL_DEVICE Matrix<T, N> exp(Matrix<T, N> const &a, int order = 12) noexcept
{
	// use exp(A) = exp(A/16)^16 with 12th-order Taylor
	// TODO (ideas):
	//    * handle the trace of a separately (and exactly)
	//    * choose the expansion order depending on the norm of a
	//    * use exact formulas for small N
	Matrix<T, N> b = a * (1.0 / 16.0);
	Matrix<T, N> r = Matrix<T, N>::identity() + b;
	for (int n = 2; n <= order; ++n)
	{
		b = a * b * (1.0 / (16.0 * n));
		r += b;
	}
	for (int i = 0; i < 4; ++i)
		r = r * r;
	return r;
}

/**
 * Random point on a sphere with uniform distribution.
 */
template <class T> class uniform_sphere_distribution;

template <class RealType, int N>
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
		for (int i = 0; i < N; ++i)
			r(i) = normal(rng);
		return normalize(r);
	}
};

template <class T> class exponential_sphere_distribution;

/**
 * Random point on a sphere with distribution P(r) = e^(-alpha r[0]).
 */
template <class RealType>
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
template <class T, int N> struct formatter<util::Matrix<T, N>> : formatter<T>
{
	// NOTE: parse() is inherited from formatter<T>

	template <class FormatContext>
	auto format(const util::Matrix<T, N> &a, FormatContext &ctx)
	    -> decltype(ctx.out())
	{
		format_to(ctx.out(), "[[");
		for (int i = 0; i < N; ++i)
		{
			for (int j = 0; j < N; ++j)
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
