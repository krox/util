#pragma once

#include "util/hash.h"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace util {

// A tiny lock-free map implementation for multithreaded scenarios
//   * Fixed capacity: currently 256 entries, could be made configurable
//   * No deletion: Just insertion and lookup
//   * Unordered: every lookup is a linear scan
//   * Lock-free: insertions and lookups can run concurrently without locks
template <class Key, class T> class fixed_map
{
  public:
	using key_type = Key;
	using mapped_type = T;
	using value_type = std::pair<const Key, T>;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using pointer = value_type *;
	using const_pointer = value_type const *;

	static constexpr size_t static_capacity = 256;

	fixed_map() noexcept = default;
	fixed_map(fixed_map const &) = delete;
	fixed_map &operator=(fixed_map const &) = delete;
	fixed_map(fixed_map &&) = delete;
	fixed_map &operator=(fixed_map &&) = delete;

	~fixed_map()
	{
		auto slots = slots_.load(std::memory_order_relaxed);
		if (!slots)
			return;

		size_t n = size_.load(std::memory_order_relaxed);
		for (size_t i = 0; i < n; ++i)
			delete slots[i];
		delete[] slots;
	}

	bool empty() const noexcept { return size() == 0; }
	size_t size() const noexcept
	{
		return size_.load(std::memory_order_acquire);
	}
	constexpr size_t capacity() const noexcept { return static_capacity; }

	std::span<pointer const> items() noexcept
	{
		auto slots = slots_.load(std::memory_order_acquire);
		size_t n = size_.load(std::memory_order_acquire);
		if (!slots || n == 0)
			return {};
		return std::span<pointer const>(slots, n);
	}

	std::span<const_pointer const> items() const noexcept
	{
		auto slots = slots_.load(std::memory_order_acquire);
		size_t n = size_.load(std::memory_order_acquire);
		if (!slots || n == 0)
			return {};
		return std::span<const_pointer const>(slots, n);
	}

	bool contains(in_param<Key> key) const noexcept
	{
		return find(key) != nullptr;
	}
	size_t count(in_param<Key> key) const noexcept { return contains(key); }

	T *find(in_param<Key> key) noexcept
	{
		if (auto value = find_value(key))
			return &value->second;
		return nullptr;
	}

	T const *find(in_param<Key> key) const noexcept
	{
		if (auto value = find_value(key))
			return &value->second;
		return nullptr;
	}

	T &at(in_param<Key> key)
	{
		if (auto value = find(key))
			return *value;
		throw std::out_of_range("key not found in util::fixed_map");
	}

	T const &at(in_param<Key> key) const
	{
		if (auto value = find(key))
			return *value;
		throw std::out_of_range("key not found in util::fixed_map");
	}

	T &operator[](in_param<Key> key)
	    requires std::default_initializable<T>
	{
		return try_emplace(key).first;
	}

	template <class... Args>
	std::pair<T &, bool> try_emplace(in_param<Key> key, Args &&...args)
	{
		auto slots = ensure_slots();
		std::unique_ptr<value_type> candidate;

		for (size_t i = 0; i < static_capacity; ++i)
		{
			auto slot = slot_ref(slots[i]);
			pointer current = slot.load(std::memory_order_acquire);
			if (current)
			{
				if (current->first == key)
					return {current->second, false};
				continue;
			}

			if (!candidate)
				candidate = std::make_unique<value_type>(
				    key, T(std::forward<Args>(args)...));

			pointer expected = nullptr;
			if (slot.compare_exchange_strong(expected, candidate.get(),
			                                 std::memory_order_release,
			                                 std::memory_order_acquire))
			{
				size_.fetch_add(1, std::memory_order_release);
				pointer inserted = candidate.release();
				return {inserted->second, true};
			}

			if (expected->first == key)
				return {expected->second, false};
		}

		throw std::runtime_error("util::fixed_map is full");
	}

	std::pair<T &, bool> insert(value_type const &value)
	{
		return try_emplace(value.first, value.second);
	}

	std::pair<T &, bool> insert(value_type &&value)
	{
		return try_emplace(value.first, std::move(value.second));
	}

  private:
	static std::atomic_ref<pointer> slot_ref(pointer &slot) noexcept
	{
		return std::atomic_ref<pointer>(slot);
	}

	pointer *ensure_slots()
	{
		auto slots = slots_.load(std::memory_order_acquire);
		if (slots)
			return slots;

		auto candidate =
		    std::unique_ptr<pointer[]>(new pointer[static_capacity]{});
		auto raw = candidate.get();
		if (slots_.compare_exchange_strong(slots, raw,
		                                   std::memory_order_release,
		                                   std::memory_order_acquire))
		{
			candidate.release();
			return raw;
		}

		return slots;
	}

	pointer find_value(in_param<Key> key) noexcept
	{
		auto slots = slots_.load(std::memory_order_acquire);
		if (!slots)
			return nullptr;

		for (size_t i = 0; i < static_capacity; ++i)
		{
			pointer current =
			    slot_ref(slots[i]).load(std::memory_order_acquire);
			if (!current)
				return nullptr;
			if (current->first == key)
				return current;
		}
		return nullptr;
	}

	const_pointer find_value(in_param<Key> key) const noexcept
	{
		auto slots = slots_.load(std::memory_order_acquire);
		if (!slots)
			return nullptr;

		for (size_t i = 0; i < static_capacity; ++i)
		{
			pointer current =
			    slot_ref(slots[i]).load(std::memory_order_acquire);
			if (!current)
				return nullptr;
			if (current->first == key)
				return current;
		}
		return nullptr;
	}

	std::atomic<pointer *> slots_{nullptr};
	std::atomic<size_t> size_{0};
};

} // namespace util