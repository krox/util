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

/** N-dimensional array view */
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
	index_type shape_;
	index_type stride_;

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
	ndspan(T *data, index_type shape, index_type stride)
	    : data_(data), shape_(shape), stride_(stride)
	{}

	// "const ndspan<T>" to "ndspan<const T>" conversion
	ndspan(const ndspan<value_type, N> &v)
	    : data_(v.data_), shape_(v.shape_), stride_(v.stride_)
	{}

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
	template <typename... Is>
	auto operator()(Is... is) ->
	    typename std::conditional<sizeof...(Is) == N, T &,
	                              ndspan<T, N - sizeof...(Is)>>::type
	{
		constexpr size_t K = sizeof...(Is);
		static_assert(K <= N);

		std::array<size_t, K> index = {is...};

		auto data = data_;
		for (int i = 0; i < K; ++i)
		{
			assert(index[i] < shape_[i]);
			data += stride_[i] * index[i];
		}

		if constexpr (K == N)
		{
			return *data;
		}
		else
		{
			std::array<size_t, N - K> shape, stride;
			for (int i = K; i < N; ++i)
			{
				shape[i - K] = shape_[i];
				stride[i - K] = stride_[i];
			}
			return ndspan<T, N - K>(data, shape, stride);
		}
	}

	template <typename... Is> auto operator()(Is... is) const
	{
		return ndspan<const T, N>(data_, shape_, stride_)(is...);
	}
};

template <size_t N, typename F, typename... Ts>
void map(F &&f, ndspan<Ts, N>... as)
{
	// TODO: check that shapes match
	size_t len = std::get<0>(std::make_tuple(as.shape(0)...));

	if constexpr (N == 1)
	{
		for (size_t i = 0; i < len; ++i)
			f(as(i)...);
	}
	else
		for (size_t i = 0; i < len; ++i)
			map(f, as(i)...);
}

/** create a (row-major) Nd-array-view from a 1d array view */
template <typename T, size_t N>
ndspan<T, N> make_ndspan(gspan<T> data, std::array<size_t, N> shape)
{
	size_t count = 1;
	for (size_t s : shape)
		count *= s;
	assert(count == data.size());

	std::array<size_t, N> stride;
	stride[N - 1] = data.stride();
	for (int i = N - 2; i >= 0; --i)
		stride[i] = stride[i + 1] * shape[i + 1];

	return ndspan<T, N>(data.data(), shape, stride);
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
		auto strs = util::make_ndspan<std::string>(str_buf, a.shape());
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
