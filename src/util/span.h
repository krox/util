#ifndef UTIL_SPAN_H
#define UTIL_SPAN_H

#include "fmt/format.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace util {

/** contiguous 1D array-view */
template <typename T> class span
{
	T *data_ = nullptr;
	size_t size_ = 0;

	typedef typename std::remove_cv<T>::type T_mut;

  public:
	using value_type = T;
	using size_type = size_t;
	using index_type = size_t;
	using iterator = T *;
	using const_iterator = const T *;

	/** constructors */
	span() = default;
	span(T *data, size_t size) : data_(data), size_(size) {}
	span(T *begin, T *end) : data_(begin), size_(end - begin) {}
	span(std::vector<T> &v) : data_(v.data()), size_(v.size()) {}
	span(const std::vector<T_mut> &v) : data_(v.data()), size_(v.size()) {}
	span(const span<T_mut> &v) : data_(v.data()), size_(v.size()) {}
	template <size_t N>
	span(std::array<T, N> &v) : data_(v.data()), size_(v.size())
	{}
	template <size_t N>
	span(const std::array<T_mut, N> &v) : data_(v.data()), size_(v.size())
	{}

	/** field access */
	T *data() { return data_; }
	const T *data() const { return data_; }
	size_t size() const { return size_; }

	/** element access */
	T &operator[](size_t i) { return data_[i]; }
	const T &operator[](size_t i) const { return data_[i]; }
	T &operator()(int i) { return data_[i]; }
	const T &operator()(int i) const { return data_[i]; }

	/** iterators */
	iterator begin() { return data_; }
	iterator end() { return data_ + size_; }
	const_iterator begin() const { return data_; }
	const_iterator end() const { return data_ + size_; }
	const_iterator cbegin() const { return data_; }
	const_iterator cend() const { return data_ + size_; }

	/** number of bytes */
	size_t size_bytes() const { return size_ * sizeof(T); }

	/** supspan */
	span<T> subspan(size_t a, size_t b)
	{
		return span<T>(data_ + a, data_ + b);
	}
};

template <class T> span<const std::byte> as_bytes(span<T> s)
{
	return {(const std::byte *)s.data(), s.size_bytes()};
}

template <class T> span<std::byte> as_writable_bytes(span<T> s)
{
	return {(std::byte *)s.data(), s.size_bytes()};
}

/** strided 1D array-view */
template <typename T> class gspan
{
	T *data_ = nullptr;
	size_t size_ = 0;
	size_t stride_ = 1;

	typedef typename std::remove_cv<T>::type T_mut;

  public:
	using value_type = T;
	using size_type = size_t;
	using index_type = size_t;
	// NOTE: don't use T* as iterator

	/** constructors */
	gspan() = default;
	gspan(T *data, size_t size, size_t stride)
	    : data_(data), size_(size), stride_(stride)
	{}
	gspan(std::vector<T> &v) : data_(v.data()), size_(v.size()) {}
	gspan(const std::vector<T_mut> &v) : data_(v.data()), size_(v.size()) {}
	gspan(span<T> &v) : data_(v.data()), size_(v.size()) {}
	gspan(const gspan<T_mut> &v)
	    : data_(v.data()), size_(v.size()), stride_(v.stride())
	{}

	/** field access */
	T *data() { return data_; }
	const T *data() const { return data_; }
	size_t size() const { return size_; }
	size_t stride() const { return stride_; }

	/** element access */
	T &operator[](size_t i) { return data_[i * stride_]; }
	const T &operator[](size_t i) const { return data_[i * stride_]; }
	T &operator()(int i) { return data_[i * stride_]; }
	const T &operator()(int i) const { return data_[i * stride_]; }

	/** supspan */
	gspan<T> subspan(size_t a, size_t b)
	{
		assert(a <= b && b <= size_);
		return gspan<T>(data_ + a * stride_, b - a, stride_);
	}
};

/** forward declarations */
template <typename, size_t> class ndspan;
template <size_t N, typename F, typename... Ts>
void map(F &&, ndspan<Ts, N>...);

/** wildcard index */
class
{
} _;

/** slice index */
struct Slice
{
	size_t begin = 0, end = 0, step = 1;
	Slice() = default;
	Slice(size_t begin_, size_t end_) : begin(begin_), end(end_), step(1) {}
	Slice(size_t begin_, size_t end_, size_t step_)
	    : begin(begin_), end(end_), step(step_)
	{}
};

/**
 * N-dimensional array view
 *    - non-owning
 *    - arbitrary strides, though row-major is default upon construction
 *    - out-of-bounds element access is undefined behaviour
 *    - data-parallel operations throw on shape mismatch
 *    - TODO:
 *       * numpy-style broadcasting rules
 *       * optimize data-parallel operations (use (T)BLIS for serious stuff)
 *       * merge the N=0 case with the gspan class
 */
template <typename T, size_t N> class ndspan
{
	/** rank-0 tensors "should" just be scalar values. But implementing
	 *  that leads to somewhat weird corner-cases, so we don't */
	static_assert(N > 0);
	static_assert(N < 100);

  public:
	using element_type = T;
	using value_type = std::remove_cv_t<T>;
	using index_type = std::array<size_t, N>;
	using pointer = element_type *;
	using reference = element_type &;
	using size_type = size_t;

  private:
	T *data_ = nullptr;
	index_type shape_ = {};
	index_type stride_ = {};

	size_t flat_index(index_type index)
	{
		size_t r = 0;
		for (int i = 0; i < N; ++i)
		{
			// bounds-check on by default.
			// use parallel/sliced operations for performance
			assert(index[i] < shape_[i]);

			r += index[i] * stride_[i];
		}
		return r;
	}

  public:
	/** constructors */
	ndspan() = default;
	explicit ndspan(T *data, index_type shape, index_type stride)
	    : data_(data), shape_(shape), stride_(stride)
	{}

	/** create a (row-major) Nd-array-view from a 1d array view */
	explicit ndspan(gspan<T> data, index_type shape)
	    : data_(data.data()), shape_(shape)
	{
		size_t count = 1;
		for (size_t s : shape)
			count *= s;
		assert(count == data.size());

		stride_[N - 1] = data.stride();
		for (int i = (int)N - 2; i >= 0; --i)
			stride_[i] = stride_[i + 1] * shape[i + 1];
	}

	/** "const ndspan<T>" to "ndspan<const T>" conversion */
	ndspan(const ndspan<value_type, N> &v)
	    : data_(v.data_), shape_(v.shape_), stride_(v.stride_)
	{}

	/** shallow assigment, i.e. "a = ..." */
	void operator=(ndspan other) &
	{
		data_ = other.data_;
		shape_ = other.shape_;
		stride_ = other.stride_;
	}

	/** element-wise assigment, i.e. "a(...) = ..." */
	void operator=(ndspan const &other) &&
	{
		map([](T &a, T const &b) { a = b; }, *this, other);
	}

	/** broadcasting assignment */
	void operator=(T const &value)
	{
		map([&value](T &a) { a = value; }, *this);
	}

	/** field access */
	T *data() { return data_; }
	const T *data() const { return data_; }
	index_type shape() const { return shape_; }
	size_t shape(size_t i) const { return shape_[i]; }
	index_type stride() const { return stride_; }
	size_t stride(size_t i) const { return stride_[i]; }
	size_t size() const
	{
		size_t s = 1;
		for (int i = 0; i < N; ++i)
			s *= shape_[i];
		return s;
	}
	bool empty() const
	{
		for (int i = 0; i < N; ++i)
			if (shape_[i] == 0)
				return true;
		return false;
	}

	/** element access */
	T &operator[](index_type i) { return data_[flat_index(i)]; }
	const T &operator[](index_type i) const { return data_[flat_index(i)]; }

	/** arbitrary slicing */
	template <size_t start> auto subscript() -> ndspan<T, N> { return *this; }

	template <size_t start, typename... Is>
	auto subscript(decltype(_), Is &&... is)
	    -> decltype(subscript<start + 1>(std::forward<Is>(is)...))
	{
		return subscript<start + 1>(std::forward<Is>(is)...);
	}

	template <size_t start, typename... Is>
	auto subscript(size_t i, Is &&... is) -> typename std::conditional<
	    N == 1, T &,
	    decltype(ndspan<T, (N == 1 ? 1 : N - 1)>().template subscript<start>(
	        std::forward<Is>(is)...))>::type
	{
		static_assert(start < N);
		assert(i < shape_[start]);
		T *data = data_ + i * stride_[start];
		if constexpr (N == 1)
		{
			return *data;
		}
		else
		{
			std::array<size_t, N - 1> shape, stride;
			for (int k = 0; k < start; ++k)
			{
				shape[k] = shape_[k];
				stride[k] = stride_[k];
			}
			for (int k = start + 1; k < N; ++k)
			{
				shape[k - 1] = shape_[k];
				stride[k - 1] = stride_[k];
			}
			return ndspan<T, N - 1>(data, shape, stride)
			    .template subscript<start>(is...);
		}
	}

	template <size_t start, typename... Is>
	auto subscript(Slice i, Is &&... is)
	    -> decltype(subscript<start + 1>(is...))
	{
		static_assert(start < N);
		assert(i.begin <= i.end && i.end <= shape_[start]);

		T *data = data_ + i.begin * stride_[start];
		std::array<size_t, N> shape = shape_;
		std::array<size_t, N> stride = stride_;
		shape[start] = (i.end - i.begin) / i.step;
		stride[start] = stride[start] * i.step;
		return ndspan<T, N>(data, shape, stride)
		    .template subscript<start + 1>(std::forward<Is>(is)...);
	}

	template <typename... Is>
	auto operator()(Is &&... is) -> decltype(subscript<0>(is...))
	{
		static_assert(sizeof...(is) <= N);
		return subscript<0>(std::forward<Is>(is)...);
	}

	template <typename... Is>
	auto operator()(Is &&... is) const -> decltype(subscript<0>(is...))
	{
		static_assert(sizeof...(is) <= N);
		return ndspan<const T, N>(data_, shape_, stride_)
		    .template subscript<0>(std::forward<Is>(is)...);
	}

	/** check if span is in contiguous/dense (row-major) format */
	bool contiguous() const
	{
		if (stride_[N - 1] != 1)
			return false;
		for (int i = (int)N - 2; i >= 0; --i)
			if (stride_[i] != stride_[i + 1] * shape_[i + 1])
				return false;
		return true;
	}

	template <size_t K>
	auto reshape(std::array<size_t, K> new_shape) -> ndspan<T, K>
	{
		assert(contiguous());
		size_t count = 1;
		for (size_t s : new_shape)
			count *= s;
		assert(count == size());

		std::array<size_t, K> new_stride;
		new_stride[K - 1] = stride_[0];
		for (int i = (int)K - 2; i >= 0; --i)
			new_stride[i] = new_stride[i + 1] * new_shape[i + 1];

		return ndspan<T, K>(data_, new_shape, new_stride);
	}

	/** data-parallel operations */
	template <typename U> void operator+=(ndspan<U, N> b)
	{
		map([](T &x, U const &y) { x += y; }, *this, b);
	}
	template <typename U> void operator-=(ndspan<U, N> b)
	{
		map([](T &x, U const &y) { x -= y; }, *this, b);
	}
	template <typename U> void operator*=(ndspan<U, N> b)
	{
		map([](T &x, U const &y) { x *= y; }, *this, b);
	}
	template <typename U> void operator/=(ndspan<U, N> b)
	{
		map([](T &x, U const &y) { x /= y; }, *this, b);
	}

	/** broadcasting data-parallel operations */
	template <typename U> void operator+=(U const &b)
	{
		map([&b](T &x) { x += b; }, *this);
	}
	template <typename U> void operator-=(U const &b)
	{
		map([&b](T &x) { x -= b; }, *this);
	}
	template <typename U> void operator*=(U const &b)
	{
		map([&b](T &x) { x *= b; }, *this);
	}
	template <typename U> void operator/=(U const &b)
	{
		map([&b](T &x) { x /= b; }, *this);
	}
};

/** deduction guides */
template <typename C, size_t N>
ndspan(C, std::array<size_t, N>)->ndspan<typename C::value_type, N>;
template <typename T, size_t N>
ndspan(T *, std::array<size_t, N>, std::array<size_t, N>)->ndspan<T, N>;

template <size_t N, typename F, typename... Ts>
void map_impl(F &&f, size_t *shape, ndspan<Ts, N>... as)
{
	if constexpr (N == 1)
	{
		for (size_t i = 0; i < *shape; ++i)
			f(as(i)...);
	}
	else
	{
		for (size_t i = 0; i < *shape; ++i)
			map_impl(f, shape + 1, as(i)...);
	}
}

template <size_t N, typename F, typename... Ts>
void map(F &&f, ndspan<Ts, N>... as)
{
	std::array<std::array<size_t, N>, sizeof...(Ts)> shapes = {as.shape()...};
	for (int i = 1; i < sizeof...(Ts); ++i)
		assert(shapes[i] == shapes[0]);
	map_impl(f, shapes[0].data(), as...);
}
} // namespace util

namespace fmt {

namespace {

template <typename Iterator>
Iterator format_string_array(Iterator it, util::ndspan<std::string, 1> a,
                             size_t pad_len, size_t indent_len)
{
	*it++ = '[';
	for (size_t i = 0; i < a.shape(0); ++i)
		it = format_to(it, (i == 0) ? "{:<{}}" : ", {:<{}}", a(i), pad_len);
	*it++ = ']';
	return it;
}

template <typename Iterator, size_t N>
Iterator format_string_array(Iterator it, util::ndspan<std::string, N> a,
                             size_t pad_len, size_t indent_len)
{
	*it++ = '[';
	for (size_t i = 0; i < a.shape(0); ++i)
	{
		it = format_string_array(it, a(i), pad_len, indent_len + 1);
		if (i != a.shape(0) - 1)
		{
			*it++ = ',';
			for (size_t k = 0; k < N - 1; ++k)
				*it++ = '\n';
			for (size_t k = 0; k < indent_len + 1; ++k)
				*it++ = ' ';
		}
	}
	*it++ = ']';
	return it;
}

} // namespace

template <typename T, size_t N>
struct formatter<util::ndspan<T, N>> : formatter<T>
{
	// NOTE: parse() is inherited from formatter<T>

	template <typename FormatContext>
	auto format(const util::ndspan<T, N> &a, FormatContext &ctx)
	    -> decltype(ctx.out())
	{
		// format all elements
		auto str_buf = std::vector<std::string>(a.size());
		auto strs = util::ndspan(str_buf, a.shape());
		// TODO: use actual formatter here
		map([&](auto &s, auto &x) { s = fmt::format("{}", x); }, strs, a);

		// determine maximum length for nice alignment
		size_t pad_len = 0;
		for (auto const &s : str_buf)
			pad_len = std::max(pad_len, s.size());

		// format the array
		return format_string_array(ctx.out(), strs, pad_len, 0);
	}
};

} // namespace fmt

#endif
