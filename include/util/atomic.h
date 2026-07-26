#pragma once

// Simple atomic primitives for lock-free shared datastructures.

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace util {

template <class T>
concept atomic_fetch_addable = requires(std::atomic<T> &value, T delta) {
	{ value.fetch_add(delta, std::memory_order_relaxed) } -> std::same_as<T>;
};

template <class T>
concept atomic_fetch_subtractable = requires(std::atomic<T> &value, T delta) {
	{ value.fetch_sub(delta, std::memory_order_relaxed) } -> std::same_as<T>;
};

// Thin convenience wrapper around 'std::atomic' that uses relaxed memory
// ordering for all operations.
//   * To be used for atomic variables that are not used for synchronization of
//     other data, e.g., performance counters and statistics.
//   * Rejects types that are not actually lock-free. Just to avoid surprises.
//   * Specializations for 'std::chrono::duration/time_point'. Rationale:
//     While a naive 'std::atomic<std::chrono::duration>' works fine (and is
//     typically lock-free), it lacks arithmetic operators '+=', '-='.
//     util::relaxed_atomic<std::chrono::duration> provides those. Same for
//     'std::chrono::time_point'.
template <class T>
    requires(std::is_trivially_copyable_v<T> &&
             std::atomic<T>::is_always_lock_free)
class relaxed_atomic
{
	std::atomic<T> value_{};

  public:
	using value_type = T;
	static constexpr bool is_always_lock_free = true;

	constexpr relaxed_atomic() noexcept = default;
	constexpr relaxed_atomic(T desired) noexcept : value_(desired) {}

	relaxed_atomic(relaxed_atomic const &) = delete;
	relaxed_atomic &operator=(relaxed_atomic const &) = delete;
	relaxed_atomic(relaxed_atomic &&) = delete;
	relaxed_atomic &operator=(relaxed_atomic &&) = delete;

	bool is_lock_free() const noexcept { return value_.is_lock_free(); }

	void store(T desired) noexcept
	{
		value_.store(desired, std::memory_order_relaxed);
	}

	T load() const noexcept { return value_.load(std::memory_order_relaxed); }
	operator T() const noexcept { return load(); }

	T operator=(T desired) noexcept
	{
		store(desired);
		return desired;
	}

	T exchange(T desired) noexcept
	{
		return value_.exchange(desired, std::memory_order_relaxed);
	}

	bool compare_exchange_weak(T &expected, T desired) noexcept
	{
		return value_.compare_exchange_weak(expected, desired,
		                                    std::memory_order_relaxed,
		                                    std::memory_order_relaxed);
	}

	bool compare_exchange_strong(T &expected, T desired) noexcept
	{
		return value_.compare_exchange_strong(expected, desired,
		                                      std::memory_order_relaxed,
		                                      std::memory_order_relaxed);
	}

	template <class U = T>
	    requires atomic_fetch_addable<U>
	T fetch_add(T delta) noexcept
	{
		return value_.fetch_add(delta, std::memory_order_relaxed);
	}

	template <class U = T>
	    requires atomic_fetch_subtractable<U>
	T fetch_sub(T delta) noexcept
	{
		return value_.fetch_sub(delta, std::memory_order_relaxed);
	}

	template <class U = T>
	    requires atomic_fetch_addable<U>
	T operator+=(T delta) noexcept
	{
		return fetch_add(delta) + delta;
	}

	template <class U = T>
	    requires atomic_fetch_subtractable<U>
	T operator-=(T delta) noexcept
	{
		return fetch_sub(delta) - delta;
	}
};

template <class Rep, class Period>
    requires std::atomic<Rep>::is_always_lock_free
class relaxed_atomic<std::chrono::duration<Rep, Period>>
{
	using duration_type = std::chrono::duration<Rep, Period>;

	std::atomic<Rep> value_{};

	static constexpr Rep raw(duration_type value) noexcept
	{
		return value.count();
	}
	static constexpr duration_type wrap(Rep value) noexcept
	{
		return duration_type{value};
	}

  public:
	using value_type = duration_type;
	static constexpr bool is_always_lock_free = true;

	constexpr relaxed_atomic() noexcept = default;
	constexpr relaxed_atomic(duration_type desired) noexcept
	    : value_(raw(desired))
	{}

	relaxed_atomic(relaxed_atomic const &) = delete;
	relaxed_atomic &operator=(relaxed_atomic const &) = delete;
	relaxed_atomic(relaxed_atomic &&) = delete;
	relaxed_atomic &operator=(relaxed_atomic &&) = delete;

	bool is_lock_free() const noexcept { return value_.is_lock_free(); }

	void store(duration_type desired) noexcept
	{
		value_.store(raw(desired), std::memory_order_relaxed);
	}

	duration_type load() const noexcept
	{
		return wrap(value_.load(std::memory_order_relaxed));
	}

	operator duration_type() const noexcept { return load(); }

	duration_type operator=(duration_type desired) noexcept
	{
		store(desired);
		return desired;
	}

	duration_type exchange(duration_type desired) noexcept
	{
		return wrap(value_.exchange(raw(desired), std::memory_order_relaxed));
	}

	bool compare_exchange_weak(duration_type &expected,
	                           duration_type desired) noexcept
	{
		Rep raw_expected = raw(expected);
		bool exchanged = value_.compare_exchange_weak(
		    raw_expected, raw(desired), std::memory_order_relaxed,
		    std::memory_order_relaxed);
		if (!exchanged)
			expected = wrap(raw_expected);
		return exchanged;
	}

	bool compare_exchange_strong(duration_type &expected,
	                             duration_type desired) noexcept
	{
		Rep raw_expected = raw(expected);
		bool exchanged = value_.compare_exchange_strong(
		    raw_expected, raw(desired), std::memory_order_relaxed,
		    std::memory_order_relaxed);
		if (!exchanged)
			expected = wrap(raw_expected);
		return exchanged;
	}

	duration_type fetch_add(duration_type delta) noexcept
	{
		return wrap(value_.fetch_add(raw(delta), std::memory_order_relaxed));
	}

	duration_type fetch_sub(duration_type delta) noexcept
	{
		return wrap(value_.fetch_sub(raw(delta), std::memory_order_relaxed));
	}

	duration_type operator+=(duration_type delta) noexcept
	{
		return fetch_add(delta) + delta;
	}

	duration_type operator-=(duration_type delta) noexcept
	{
		return fetch_sub(delta) - delta;
	}
};

template <class Clock, class Duration>
    requires std::atomic<typename Duration::rep>::is_always_lock_free
class relaxed_atomic<std::chrono::time_point<Clock, Duration>>
{
	using time_point_type = std::chrono::time_point<Clock, Duration>;
	using rep = typename Duration::rep;

	std::atomic<rep> value_{};

	static constexpr rep raw(time_point_type value) noexcept
	{
		return value.time_since_epoch().count();
	}

	static constexpr time_point_type wrap(rep value) noexcept
	{
		return time_point_type{Duration{value}};
	}

  public:
	using value_type = time_point_type;
	using duration_type = Duration;
	static constexpr bool is_always_lock_free = true;

	constexpr relaxed_atomic() noexcept = default;
	constexpr relaxed_atomic(time_point_type desired) noexcept
	    : value_(raw(desired))
	{}

	relaxed_atomic(relaxed_atomic const &) = delete;
	relaxed_atomic &operator=(relaxed_atomic const &) = delete;
	relaxed_atomic(relaxed_atomic &&) = delete;
	relaxed_atomic &operator=(relaxed_atomic &&) = delete;

	bool is_lock_free() const noexcept { return value_.is_lock_free(); }

	void store(time_point_type desired) noexcept
	{
		value_.store(raw(desired), std::memory_order_relaxed);
	}

	time_point_type load() const noexcept
	{
		return wrap(value_.load(std::memory_order_relaxed));
	}

	operator time_point_type() const noexcept { return load(); }

	time_point_type operator=(time_point_type desired) noexcept
	{
		store(desired);
		return desired;
	}

	time_point_type exchange(time_point_type desired) noexcept
	{
		return wrap(value_.exchange(raw(desired), std::memory_order_relaxed));
	}

	bool compare_exchange_weak(time_point_type &expected,
	                           time_point_type desired) noexcept
	{
		rep raw_expected = raw(expected);
		bool exchanged = value_.compare_exchange_weak(
		    raw_expected, raw(desired), std::memory_order_relaxed,
		    std::memory_order_relaxed);
		if (!exchanged)
			expected = wrap(raw_expected);
		return exchanged;
	}

	bool compare_exchange_strong(time_point_type &expected,
	                             time_point_type desired) noexcept
	{
		rep raw_expected = raw(expected);
		bool exchanged = value_.compare_exchange_strong(
		    raw_expected, raw(desired), std::memory_order_relaxed,
		    std::memory_order_relaxed);
		if (!exchanged)
			expected = wrap(raw_expected);
		return exchanged;
	}

	time_point_type fetch_add(duration_type delta) noexcept
	{
		return wrap(value_.fetch_add(delta.count(), std::memory_order_relaxed));
	}

	time_point_type fetch_sub(duration_type delta) noexcept
	{
		return wrap(value_.fetch_sub(delta.count(), std::memory_order_relaxed));
	}

	time_point_type operator+=(duration_type delta) noexcept
	{
		return fetch_add(delta) + delta;
	}

	time_point_type operator-=(duration_type delta) noexcept
	{
		return fetch_sub(delta) - delta;
	}
};

} // namespace util