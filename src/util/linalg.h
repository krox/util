#pragma once

#include <array>
#include <cassert>
#include <initializer_list>

/**
vector/matrix types for fixed (small) dimension. Similar to the GLM library.

Some general notes:
 * The basic type 'T' can be any numerical type which supports arithmetic
   itself. Not just float/double as in GLM.
 * Mixed-Type operations are supported like Vector<double> + Vector<float>
 * The compiler might be able to do some limited auto-vectorization.
   But for serious workloads, you probably need something like 'Vector<simd<T>>'
   using std::experimental::simd, or any available simd-wrapper library.
 * Complex types are not supported yet. (It compiles, but does not do any
   complex conjugation inside inner products and such.)

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

template <typename T, size_t N> struct Vector;

template <typename T> struct Vector<T, 2>
{
	T x, y;

	Vector() = default;
	Vector(T const &a, T const &b) : x(a), y(b) {}

	T &operator[](size_t i)
	{
		assert(i < 2);
		return (&x)[i];
	}
	T const &operator[](size_t i) const
	{
		assert(i < 2);
		return (&x)[i];
	}
};

template <typename T> struct Vector<T, 3>
{
	T x, y, z;

	Vector() = default;
	Vector(T const &a, T const &b, T const &c) : x(a), y(b), z(c) {}

	T &operator[](size_t i)
	{
		assert(i < 3);
		return (&x)[i];
	}
	T const &operator[](size_t i) const
	{
		assert(i < 3);
		return (&x)[i];
	}
};

template <typename T> struct Vector<T, 4>
{
	T x, y, z, w;

	Vector() = default;
	Vector(T const &a, T const &b, T const &c, T const &d)
	    : x(a), y(b), z(c), w(d)
	{}

	T &operator[](size_t i)
	{
		assert(i < 4);
		return (&x)[i];
	}
	T const &operator[](size_t i) const
	{
		assert(i < 4);
		return (&x)[i];
	}
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

/**
 * Geometrical functions with naming convention as in GLSL/GLM.
 */
template <typename T, typename U, size_t N>
auto dot(Vector<T, N> const &a, Vector<U, N> const &b) -> decltype(a[0] * b[0])
{
	decltype(a[0] * b[0]) r = a[0] * b[0];
	for (size_t i = 1; i < N; ++i)
		r += a[i] * b[i];
	return r;
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
template <typename T, size_t N> T length(Vector<T, N> const &a)
{
	return sqrt(dot(a, a));
}
template <typename T, size_t N> Vector<T, N> normalize(Vector<T, N> const &a)
{
	return a * (T(1) / length(a));
}
template <typename T, typename U, size_t N>
Vector<T, N> reflect(Vector<T, N> const &a, Vector<U, N> const &normal)
{
	return a - 2 * dot(normal, a) * normal;
}

/**
 * Geometrical functions with naming convention as in Lattice QCD.
 */
template <typename T, typename U, size_t N>
auto inner_product(Vector<T, N> const &a, Vector<U, N> const &b)
    -> decltype(a[0] * b[0])
{
	return dot(a, b);
}
template <typename T, size_t N> T norm2(Vector<T, N> const &a)
{
	return inner_product(a, a);
}

template <typename T, size_t N> class Matrix
{
	/** only square matrices (yet) */
	std::array<T, N * N> data_;

  public:
	Matrix() = default;
	Matrix(T const &a)
	{
		data_.fill(T(0));
		for (size_t i = 0; i < N; ++i)
			data_[i * (N + 1)] = a;
	}

	T &operator()(size_t i, size_t j)
	{
		assert(i < N && j < N);
		return data_[i * N + j];
	}
	T const &operator()(size_t i, size_t j) const
	{
		assert(i < N && j < N);
		return data_[i * N + j];
	}

	T *data() { return data_.data(); }
	T const *data() const { return data_.data(); }
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

template <typename T, size_t N> Matrix<T, N> transpose(Matrix<T, N> const &a)
{
	Matrix<T, N> r;
	for (size_t i = 0; i < N; ++i)
		for (size_t j = 0; j < N; ++j)
			r(i, j) = a(j, i);
	return r;
}

template <typename T> Matrix<T, 3> inverse(Matrix<T, 3> const &a)
{
	T det = a(0, 0) * (a(1, 1) * a(2, 2) - a(2, 1) * a(1, 2)) -
	        a(0, 1) * (a(1, 0) * a(2, 2) - a(1, 2) * a(2, 0)) +
	        a(0, 2) * (a(1, 0) * a(2, 1) - a(1, 1) * a(2, 0));

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

	return b * (T(1) / det);
}

} // namespace util
