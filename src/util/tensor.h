#ifndef UTIL_TENSOR_H
#define UTIL_TENSOR_H

#include "util/span.h"

namespace util {

/** n-dimensional array */
template <typename T, size_t N> class tensor
{
	static_assert(std::is_trivially_copyable_v<T>);
	static_assert(N > 0);
	static_assert(N < 100);

	ndspan<T, N> span_ = {};

  public:
	using element_type = T;
	using value_type = std::remove_cv_t<T>;
	using index_type = std::array<size_t, N>;
	using pointer = element_type *;
	using reference = element_type &;
	using size_type = size_t;

	/** default/explicit constructors */
	tensor() = default;
	explicit tensor(index_type shape)
	{
		size_t ss = 1;
		for (size_t s : shape)
			ss *= s;
		auto buf = span(new T[ss], ss);
		span_ = ndspan(buf, shape);
	}

	/** copy/move constructors */
	tensor(const tensor &other) = delete;
	tensor(tensor &&other) = delete;

	/** copy/move assigment */
	tensor &operator=(const tensor &other) = delete;
	tensor &operator=(tensor &&other) = delete;

	~tensor()
	{
		if (span_.data() != nullptr)
			delete[] span_.data();
	}

	/** field access */
	T *data() { return span_.data(); }
	const T *data() const { return span_.data(); }
	index_type shape() const { return span_.shape(); }
	size_t shape(size_t i) const { return span_.shape(i); }
	index_type stride() const { return span_.stride(); }
	size_t stride(size_t i) const { return span_.stride(i); }
	size_t size() const { return span_.size(); }
	bool empty() const { return span_.empty(); }

	/** element access */
	T &operator[](index_type i) { return span_[i]; }
	const T &operator[](index_type i) const { return span_[i]; }

	template <typename... Is>
	auto operator()(Is &&... is) -> decltype(span_(std::forward<Is>(is)...))
	{
		static_assert(sizeof...(is) <= N);
		return span_(std::forward<Is>(is)...);
	}

	template <typename... Is>
	auto operator()(Is &&... is) const
	    -> decltype(span_(std::forward<Is>(is)...))
	{
		static_assert(sizeof...(is) <= N);
		return span_(std::forward<Is>(is)...);
	}

	template <size_t K> auto reshape(std::array<size_t, K> new_shape)
	{
		return span_.reshape(new_shape);
	}
};

} // namespace util

#endif
