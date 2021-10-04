#ifndef UTIL_SPAN_H
#define UTIL_SPAN_H

#include "fmt/format.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace util {

/**
 * NOTES:
 *   - 'const span<T>' only means "head const", i.e. the elements are still
 *     mutable (unless T itself is const). This is the same a simple pointer,
 *     but different from 'std::vector'
 *   - .subspan() takes start+size, NOT start+end as arguments. This is the same
 *     as 'std::string.substr()' but different from other languages like
 *     D (builtin slice-type), python (lists, numpy-arrays), ...
 */

/** Contiguous 1D array-view. Will be superseded by std::span in C++20 */
template <typename T> class span
{
	T *data_ = nullptr;
	size_t size_ = 0;

  public:
	using element_type = T;
	using value_type = std::remove_cv_t<T>;
	using size_type = size_t;
	using index_type = size_t;
	using iterator = T *;

	/** constructors */
	span() = default;
	span(T *data, size_t size) : data_(data), size_(size) {}
	span(T *begin, T *end) : data_(begin), size_(end - begin) {}
	span(std::vector<T> &v) : data_(v.data()), size_(v.size()) {}
	span(std::vector<value_type> const &v) : data_(v.data()), size_(v.size()) {}
	span(span<value_type> const &v) : data_(v.data()), size_(v.size()) {}
	template <size_t N>
	span(std::array<T, N> &v) : data_(v.data()), size_(v.size())
	{}
	template <size_t N>
	span(std::array<value_type, N> const &v) : data_(v.data()), size_(v.size())
	{}
	span(std::initializer_list<T> data) : span(data.begin(), data.end()) {}
	template <size_t N> span(T (&data)[N]) : span(data, N) {}

	/** field access */
	T *data() const { return data_; }
	size_t size() const { return size_; }

	/** element access */
	T &operator[](size_t i) const { return data_[i]; }
	T &operator()(int i) const { return data_[i]; }

	/** iterators */
	iterator begin() const { return data_; }
	iterator end() const { return data_ + size_; }

	/** number of bytes */
	size_t size_bytes() const { return size_ * sizeof(T); }

	/** supspan */
	span<T> slice(size_t a, size_t b) const
	{
		return span<T>(data_ + a, data_ + b);
	}
};

template <class T> span<const std::byte> as_bytes(span<T> s)
{
	return {(const std::byte *)s.data(), s.size_bytes()};
}

template <class T> span<const std::byte> as_bytes(std::basic_string_view<T> s)
{
	return {(const std::byte *)s.data(), s.size() * sizeof(T)};
}

inline span<const std::byte> as_bytes(char const *s)
{
	return as_bytes(std::string_view(s));
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

  public:
	using element_type = T;
	using value_type = std::remove_cv_t<T>;
	using size_type = size_t;
	using index_type = size_t;
	// NOTE: don't use T* as iterator

	/** constructors */
	gspan() = default;
	gspan(T *data, size_t size, size_t stride)
	    : data_(data), size_(size), stride_(stride)
	{}
	gspan(std::vector<T> &v) : data_(v.data()), size_(v.size()) {}
	gspan(std::vector<value_type> const &v) : data_(v.data()), size_(v.size())
	{}
	gspan(span<T> &v) : data_(v.data()), size_(v.size()) {}
	gspan(gspan<value_type> const &v)
	    : data_(v.data()), size_(v.size()), stride_(v.stride())
	{}

	/** field access */
	T *data() const { return data_; }
	size_t size() const { return size_; }
	size_t stride() const { return stride_; }

	/** element access */
	T &operator[](size_t i) const { return data_[i * stride_]; }
	T &operator()(int i) const { return data_[i * stride_]; }

	/** supspan */
	gspan<T> slice(size_t a, size_t b) const
	{
		assert(a <= b && b <= size_);
		return gspan<T>(data_ + a * stride_, b - a, stride_);
	}
};

/** short-hand for the "erase–remove idiom" (part of std in C++20) */
template <class T, class Alloc, class U>
constexpr typename std::vector<T, Alloc>::size_type
erase(std::vector<T, Alloc> &c, const U &value)
{
	auto it = std::remove(c.begin(), c.end(), value);
	auto r = std::distance(it, c.end());
	c.erase(it, c.end());
	return r;
}

template <class T, class Alloc, class Pred>
constexpr typename std::vector<T, Alloc>::size_type
erase_if(std::vector<T, Alloc> &c, Pred pred)
{
	auto it = std::remove_if(c.begin(), c.end(), pred);
	auto r = std::distance(it, c.end());
	c.erase(it, c.end());
	return r;
}

/** forward declarations */
template <typename, size_t> class ndspan;
template <size_t N, typename F, typename... Ts>
void map(F &&, ndspan<Ts, N> const &...);

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
 * Helper for ndspan.operator(). Counts how many of the indices are 'size_t'.
 * Has to distinguish between 'size_t', 'decltype(_)' and 'Slice'.
 * TODO: take implicit conversions into account like 'const Slice' -> 'Slice'
 *       and any integer-type -> size_t.
 */
template <typename...> struct count_indices;
template <> struct count_indices<>
{
	static constexpr size_t value = 0;
};
template <typename... Is> struct count_indices<size_t, Is...>
{
	static constexpr size_t value = 1 + count_indices<Is...>::value;
};
template <typename... Is> struct count_indices<int, Is...>
{
	static constexpr size_t value = 1 + count_indices<Is...>::value;
};
template <typename... Is> struct count_indices<decltype(_), Is...>
{
	static constexpr size_t value = count_indices<Is...>::value;
};
template <typename... Is> struct count_indices<Slice, Is...>
{
	static constexpr size_t value = count_indices<Is...>::value;
};
template <typename... Is>
inline constexpr size_t count_indices_v = count_indices<Is...>::value;

/** helper for arbitrary slicing (kinda silly naming) */
template <typename T, size_t N> struct ndspan_type
{
	using type = ndspan<T, N>;
};
template <typename T> struct ndspan_type<T, 0>
{
	using type = T &;
};
template <typename T, size_t N>
using ndspan_type_t = typename ndspan_type<T, N>::type;

/**
 * N-dimensional array view
 *    - non-owning
 *    - arbitrary strides, though row-major is default upon construction
 *    - out-of-bounds element access is undefined behaviour
 *    - data-parallel operations throw on shape mismatch
 *    - TODO:
 *       * numpy-style broadcasting rules
 *       * optimize data-parallel operations (use (T)BLIS for serious stuff)
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

  public:
	/** constructors */
	ndspan() = default;
	explicit ndspan(T *data, index_type shape, index_type stride)
	    : data_(data), shape_(shape), stride_(stride)
	{}

	/** create a (row-major) Nd-array-view from a 1d array view */
	explicit ndspan(span<T> data, index_type shape)
	    : data_(data.data()), shape_(shape)
	{
		size_t count = 1;
		for (size_t s : shape)
			count *= s;
		assert(count == data.size());

		stride_[N - 1] = 1;
		for (int i = (int)N - 2; i >= 0; --i)
			stride_[i] = stride_[i + 1] * shape[i + 1];
	}

	/** "const ndspan<T>" to "ndspan<const T>" conversion */
	ndspan(ndspan<value_type, N> const &v)
	    : data_(v.data()), shape_(v.shape()), stride_(v.stride())
	{}

	/** shallow assigment, i.e. "a = ..." */
	void operator=(ndspan other) &
	{
		data_ = other.data_;
		shape_ = other.shape_;
		stride_ = other.stride_;
	}

	/** element-wise assigment, i.e. "a(...) = ..." */
	void operator=(ndspan const &other) const &&
	{
		map([](T &a, T const &b) { a = b; }, *this, other);
	}

	/** broadcasting assignment */
	void operator=(T const &value) const
	{
		map([&value](T &a) { a = value; }, *this);
	}

	/** field access */
	T *data() const { return data_; }
	index_type shape() const { return shape_; }
	size_t shape(size_t i) const { return shape_[i]; }
	index_type stride() const { return stride_; }
	size_t stride(size_t i) const { return stride_[i]; }
	size_t size() const
	{
		size_t s = 1;
		for (size_t i = 0; i < N; ++i)
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
	T &operator[](index_type index) const
	{
		size_t r = 0;
		for (int i = 0; i < N; ++i)
		{
			// bounds-check on by default.
			// use parallel/sliced operations for performance
			assert(index[i] < shape_[i]);
			r += index[i] * stride_[i];
		}
		return data_[r];
	}

	/** subspan */
	ndspan<T, N> subspan(index_type offset, index_type count) const
	{
		T *new_data = data_;

		for (size_t i = 0; i < N; ++i)
		{
			assert(offset[i] < shape_[i] && offset[i] + count[i] < shape_[i]);
			data_ += offset[i] * stride_[i];
		}

		return ndspan<T, N>(new_data, count, stride_);
	}

	/** internal helper, dont use directly */
	template <size_t start> auto subscript() const -> ndspan<T, N>
	{
		// static_assert(start <= N);
		return *this;
	}
	template <size_t start, typename... Is>
	auto subscript(size_t index, Is... is) const
	    -> ndspan_type_t<T, N - 1 - count_indices_v<Is...>>
	{
		if constexpr (N == 1)
		{
			static_assert(start == 0);
			static_assert(sizeof...(is) == 0);
			return *(data_ + stride_[0] * index);
		}
		else
		{
			static_assert(start < N);
			assert(index < shape_[start]);
			T *new_data = data_ + stride_[start] * index;
			std::array<size_t, N - 1> new_shape, new_stride;
			for (size_t i = 0; i < start; ++i)
			{
				new_shape[i] = shape_[i];
				new_stride[i] = stride_[i];
			}
			for (size_t i = start + 1; i < N; ++i)
			{
				new_shape[i - 1] = shape_[i];
				new_stride[i - 1] = stride_[i];
			}
			auto tmp = ndspan<T, N - 1>(new_data, new_shape, new_stride);
			return tmp.template subscript<start>(is...);
		}
	}
	template <size_t start, typename... Is>
	auto subscript(decltype(_), Is... is) const
	    -> ndspan_type_t<T, N - count_indices_v<Is...>>
	{
		static_assert(start < N);
		return this->subscript<start + 1>(is...);
	}
	template <size_t start, typename... Is>
	auto subscript(Slice index, Is... is) const
	    -> ndspan_type_t<T, N - count_indices_v<Is...>>
	{
		static_assert(start < N);
		assert(index.begin <= index.end && index.end <= shape_[start]);
		assert(index.step >= 1);
		T *new_data = data_ + stride_[start] * index.begin;
		std::array<size_t, N> new_shape = shape_;
		new_shape[start] =
		    (shape_[start] - index.begin + index.step - 1) / index.step;
		std::array<size_t, N> new_stride = stride_;
		new_stride[start] = stride_[start] * index.step;
		auto tmp = ndspan<T, N>(new_data, new_shape, new_stride);
		return tmp.template subscript<start + 1>(is...);
	}

	/** arbitrary slicing/indexing */
	template <typename... Is>
	auto operator()(Is &&... is) const -> decltype(subscript<0>(is...))
	{
		return subscript<0>(is...);
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
	auto reshape(std::array<size_t, K> new_shape) const -> ndspan<T, K>
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
	template <typename U> void operator+=(ndspan<U, N> const &b) const
	{
		map([](T &x, U const &y) { x += y; }, *this, b);
	}
	template <typename U> void operator-=(ndspan<U, N> const &b) const
	{
		map([](T &x, U const &y) { x -= y; }, *this, b);
	}
	template <typename U> void operator*=(ndspan<U, N> const &b) const
	{
		map([](T &x, U const &y) { x *= y; }, *this, b);
	}
	template <typename U> void operator/=(ndspan<U, N> const &b) const
	{
		map([](T &x, U const &y) { x /= y; }, *this, b);
	}

	/** broadcasting data-parallel operations */
	template <typename U> void operator+=(U const &b) const
	{
		map([&b](T &x) { x += b; }, *this);
	}
	template <typename U> void operator-=(U const &b) const
	{
		map([&b](T &x) { x -= b; }, *this);
	}
	template <typename U> void operator*=(U const &b) const
	{
		map([&b](T &x) { x *= b; }, *this);
	}
	template <typename U> void operator/=(U const &b) const
	{
		map([&b](T &x) { x /= b; }, *this);
	}
};

/** deduction guides */
template <typename C, size_t N>
ndspan(C, std::array<size_t, N>) -> ndspan<typename C::value_type, N>;
template <typename T, size_t N>
ndspan(T *, std::array<size_t, N>, std::array<size_t, N>) -> ndspan<T, N>;

template <size_t N, typename F, typename... Ts>
void map_impl(F &&f, size_t *shape, ndspan<Ts, N> const &... as)
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
void map(F &&f, ndspan<Ts, N> const &... as)
{
	std::array<std::array<size_t, N>, sizeof...(Ts)> shapes = {as.shape()...};
	for (size_t i = 1; i < sizeof...(Ts); ++i)
		assert(shapes[i] == shapes[0]);
	map_impl(f, shapes[0].data(), as...);
}

} // namespace util

namespace fmt {

namespace {

template <typename Iterator>
Iterator format_string_array(Iterator it, util::ndspan<std::string, 1> a,
                             size_t pad_len, [[maybe_unused]] size_t indent_len)
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
