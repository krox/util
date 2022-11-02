#pragma once

/**
 * Associative map implemented as hash table
 * (open adressing / closed hashing)
 *
 * compared to std::unordered_map
 *     - Does not guarantee any pointer or iterator stability accross rehashing,
 *       which can happen at any insertion (no guaranteed capacity)
 *     - Does not provide strong exception guarantees (if the value type can
 *       throw on move, or allocation fails, things are really broken)
 *     - Implemented with closed hashing and linear probing. This produces a
 *       higher number of collisions, but is good for cache locality and avoids
 *       some memory allocation overhead and defragmentation.
 *     - no interface based on "nodes" or "hints"
 *     - by default uses util::hash instead of std::hash
 *
 * TODO (maybe):
 *     - max_probe should be dynamic around O(log(n)). But then we need
 *       tombstones (or robin-hood or cuckoo) to keep average lookup O(1)
 *     - the value and control arrays should reside in a single allocation
 *     - use SIMD for checking the control bytes (this would be important in
 *       order to actually beat std::unordered_map for efficiency)
 *     - some move-avoiding version of insert, such as emplace/lazy_insert
 *     - check excepetion safety. pretty sure everything is broken if the copy-
 *       (or move-) constructor of the value_type ever fails.
 *     - should a (stateful) hasher be propagated on copy/move-assign or swap?
 *       I think no (same as most allocators), but std::unordered_map says yes.
 *
 * implementation details (might change in the future):
 *     - 7-bit hashes per entry are stored for fast linear scanning
 *       (inspired by abseil::flat_hash_map)
 *     - full hash is not stored, thus has to be recomputed on rehashes
 *     - internal capacity is always a power-of-two (with a good default hash,
 *       this should not lead to an increased number of collisions), plus
 *       max_probe-1 additional slots, so no wrap-around during probing
 *     - rehashing occurs whenever max_probe length is exceeded during insertion
 *     - there are no tombstones, so the full max_probe lenth is always searched
 */

#include "util/hash.h"
#include "util/memory.h"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <utility>

namespace util {

template <class Key, class T, class Hash = util::hash<Key>> class hash_map
{
  public:
	// types
	using key_type = Key;
	using mapped_type = T;
	using value_type = std::pair<const Key, T>;
	using hasher = Hash;

	// stateful hashers are supported, but in the current implementation they
	// are copied around with little care. Could be changed if the need arises.
	static_assert(std::is_nothrow_default_constructible_v<Hash>);
	static_assert(std::is_nothrow_copy_constructible_v<Hash>);
	static_assert(std::is_nothrow_copy_assignable_v<Hash>);

	// current implementation does not take special care for weird types
	static_assert(std::is_nothrow_move_constructible_v<key_type>);
	static_assert(std::is_nothrow_move_assignable_v<key_type>);
	static_assert(std::is_nothrow_move_constructible_v<mapped_type>);
	static_assert(std::is_nothrow_move_assignable_v<mapped_type>);

	class iterator : public std::iterator<std::forward_iterator_tag, value_type>
	{
		hash_map *map_ = nullptr;
		size_t index_ = 0;

	  public:
		iterator(hash_map *map, size_t index) noexcept
		    : map_(map), index_(index)
		{}
		value_type &operator*() const noexcept { return map_->values_[index_]; }
		value_type *operator->() const noexcept
		{
			return &map_->values_[index_];
		}
		bool operator!=(iterator other) const noexcept
		{
			return index_ != other.index_;
		}
		iterator &operator++() noexcept
		{
			++index_;
			while (index_ < map_->buckets() && map_->control_[index_] == 0)
				++index_;
			return *this;
		}
		iterator operator++(int) noexcept
		{
			auto r = *this;
			++*this;
			return r;
		}
	};

	class const_iterator
	    : public std::iterator<std::forward_iterator_tag, const value_type>
	{
		hash_map const *map_ = nullptr;
		size_t index_ = 0;

	  public:
		const_iterator(hash_map const *map, size_t index) noexcept
		    : map_(map), index_(index)
		{}
		value_type const &operator*() const noexcept
		{
			return map_->values_[index_];
		}
		value_type const *operator->() const noexcept
		{
			return &map_->values_[index_];
		}
		bool operator!=(const_iterator other) const noexcept
		{
			return index_ != other.index_;
		}
		const_iterator &operator++() noexcept
		{
			++index_;
			while (index_ < map_->buckets() && map_->control_[index_] == 0)
				++index_;
			return *this;
		}
		const_iterator operator++(int) noexcept
		{
			auto r = *this;
			++*this;
			return r;
		}
	};

	// default constructor
	hash_map() noexcept = default;

	// constructor for given number of buckets (will be rounded up)
	// NOTE: This does not guarantee any minimum capacity. With bad luck,
	//       further reallocation can happen at any time.
	explicit hash_map(size_t min_buckets, Hash const &h = Hash()) : hasher_(h)
	{
		mask_ = std::bit_ceil(std::max(min_buckets, size_t(16))) - 1;
		control_ = allocate<uint8_t>(buckets());
		values_ = allocate<value_type>(buckets());
		std::memset(control_.data(), 0, buckets());
	}

	// other constructors and special members
	explicit hash_map(Hash const &h) noexcept : hasher_(h) {}

	template <typename It>
	hash_map(It first, It last, Hash const &h = Hash()) : hasher_(h)
	{
		insert(first, last);
	}

	hash_map(std::initializer_list<value_type> ilist, Hash const &h = Hash())
	    : hash_map(ilist.begin(), ilist.end(), h)
	{}

	hash_map(hash_map const &other)
	    : hash_map(other.begin(), other.end(), other.hasher_)
	{}

	hash_map(hash_map &&other) noexcept : hasher_(other.hasher_)

	{
		swap(*this, other);
	}

	hash_map &operator=(hash_map const &other)
	{
		// NOTE: This check handles a simple 'a = a' statement, but more
		//       complicated aliasing still leads to UB. This seems to be the
		//       same compromise as is used in gcc/clang's standard library.
		//       Doing a copy+swap would solve all aliasing issues, but would
		//       loose performance in the (more common) non-aliased case.
		if (this == &other)
			return *this;

		clear();
		insert(other.begin(), other.end());
		return *this;
	}

	hash_map &operator=(hash_map &&other) noexcept
	{
		swap(*this, other);
		return *this;
	}

	hash_map &operator=(std::initializer_list<value_type> ilist)
	{
		clear();
		insert(ilist.begin(), ilist.end());
		return *this;
	}

	friend void swap(hash_map &a, hash_map &b) noexcept
	{
		assert(a.hasher_ == b.hasher_ &&
		       "cannot swap hash_map's with unequal hashers");

		using std::swap;
		swap(a.size_, b.size_);
		swap(a.mask_, b.mask_);
		swap(a.control_, b.control_);
		swap(a.values_, b.values_);
	}

	~hash_map() { clear(); }

	// iterators and general container access

	iterator begin() noexcept
	{
		size_t i = 0;
		while (i < buckets() && control_[i] == 0)
			++i;
		return iterator(this, i);
	}
	const_iterator begin() const noexcept
	{
		size_t i = 0;
		while (i < buckets() && control_[i] == 0)
			++i;
		return const_iterator(this, i);
	}
	const_iterator cbegin() const noexcept { return begin(); }

	iterator end() noexcept { return iterator(this, buckets()); }
	const_iterator end() const noexcept
	{
		return const_iterator(this, buckets());
	}
	const_iterator cend() const noexcept { return end(); }

	bool empty() const noexcept { return size_ == 0; }
	size_t size() const noexcept { return size_; };

	// remove all elements, keeps the memory
	void clear() noexcept
	{
		for (size_t i = 0; i < buckets(); ++i)
			if (control_[i])
			{
				values_[i].~value_type();
				control_[i] = 0;
			}
		size_ = 0;
	}

	// lookup

	bool contains(in_param<Key> key) const noexcept
	{
		return find_pos(key) != (size_t)-1;
	}

	size_t count(in_param<Key> key) const noexcept { return contains(key); }

	iterator find(in_param<Key> key) noexcept
	{
		if (auto i = find_pos(key); i != (size_t)-1)
			return iterator(this, i);
		else
			return end();
	}
	const_iterator find(in_param<Key> key) const noexcept
	{
		if (auto i = find_pos(key); i != (size_t)-1)
			return const_iterator(this, i);
		else
			return end();
	}

	T &at(in_param<Key> key)
	{
		auto pos = find_pos(key);
		if (pos == (size_t)-1)
			throw std::out_of_range("key not found in util::hash_map");
		return values_[pos].second;
	}

	T const &at(in_param<Key> key) const
	{
		auto pos = find_pos(key);
		if (pos == (size_t)-1)
			throw std::out_of_range("key not found in util::hash_map");
		return values_[pos].second;
	}

	T &operator[](in_param<Key> key)
	{
		return insert_impl(key, mapped_type(), false).first->second;
	}

	// insertion / removal

	std::pair<iterator, bool> insert(value_type const &value)
	{
		return insert_impl(value.first, value.second, false);
	}

	std::pair<iterator, bool> insert(value_type &&value)
	{
		return insert_impl(std::move(value.first), std::move(value.second),
		                   false);
	}

	template <class K, class V> std::pair<iterator, bool> assign(K &&k, V &&v)
	{
		return insert_impl(std::forward<K>(k), std::forward<V>(v), true);
	}

	template <typename It> void insert(It first, It last)
	{
		while (first != last)
			insert(*first++);
	}

	void insert(std::initializer_list<value_type> ilist)
	{
		insert(ilist.begin(), ilist.end());
	}

	size_t erase(in_param<Key> k) noexcept
	{
		auto pos = find_pos(k);
		if (pos == (size_t)-1)
			return 0;

		control_[pos] = 0;
		values_[pos].~value_type();
		--size_;
		return 1;
	}

	// number of buckets
	// NOTE: Due to closed hashing, this number does not guarantee any
	//       capacity-withouth-reallocation.
	size_t buckets() const noexcept { return mask_ ? mask_ + max_probe : 0; }

  private:
	// find internal position, returns -1 if not found
	size_t find_pos(in_param<Key> key, size_t base,
	                uint8_t control) const noexcept
	{
		if (size_ == 0) // catches the default-initialized empty state
			return (size_t)-1;

		base &= mask_;
		for (size_t i = base; i < base + max_probe; ++i)
			if (control_[i] == control)
				if (values_[i].first == key)
					return i;
		return (size_t)-1;
	}

	size_t find_pos(in_param<Key> key) const noexcept
	{
		auto [base, control] = hash(key);
		return find_pos(key, base, control);
	}

	template <class K, class V>
	std::pair<iterator, bool> insert_impl(K &&key, V &&value, bool overwrite)
	{
		auto [base, control] = hash(key);

		if (auto i = find_pos(key, base, control); i != (size_t)-1)
		{
			if (overwrite)
				values_[i].second = std::forward<V>(value);
			return {{this, i}, false};
		}

		if (mask_)
		{
			for (size_t i = base; i < base + max_probe; ++i)
				if (control_[i] == 0)
				{
					control_[i] = control;
					std::construct_at(&values_[i], std::forward<K>(key),
					                  std::forward<V>(value));
					++size_;
					return {{this, i}, true};
				}
		}

		// Throwing an exception is preferrable to segfaulting on a (very)
		// bad hash function. Especially since we are allowed to throw
		// (bad_alloc) here anyway.
		if (mask_ > std::max(size_ * 16, size_t(1024)))
			throw std::runtime_error("Too many collisions in util::hash_map. "
			                         "Probably the hash function is broken.");

		// At this point, we need to re-allocate. Capturing key/value by value
		// here costs an additional move, but solves some aliasing issues.
		// Should be rare enough to not matter for overall performance
		auto k = std::forward<K>(key);
		auto v = std::forward<V>(value);

		auto new_table = hash_map((mask_ + 1) * 2, hasher_);

		// TODO: All insertions into new_table are sub-optimal, because they
		//       perform unnecessary checks for duplicates. Also, hash(key) is
		//       computed twice.

		for (size_t k = 0; k < buckets(); ++k)
			if (control_[k])
			{
				// NOTE: with bad luck, new_table might do further
				//       re-allocations internally
				bool success = new_table.insert(std::move(values_[k])).second;
				(void)success;
				assert(success);
			}

		swap(*this, new_table);

		return insert_impl(std::move(k), std::move(v), overwrite);
	}

	std::pair<uint64_t, uint8_t> hash(in_param<Key> k) const noexcept
	{
		uint64_t h = hasher_(k);

		// Split into position and control byte.
		// * the control byte may not be zero (indicates an empty slot)
		// * the default hash algorithm (FNV1a) has some noticable weaknesses:
		//       * The high bits are not affected by a single-byte input at all
		//       * The lowest bit is just parity of lowest bits if key bytes
		//   The bits [29...1] are sufficiently well behaved as far as I know,
		//   so the current implementation here is probably okay.
		return {(h >> 8) & mask_, (h & 255) | 1};
	}

	static constexpr size_t max_probe = 16;

	size_t size_ = 0;
	size_t mask_ = 0;
	// TODO: merging control_ and values_ into a single allocation
	unique_memory<uint8_t> control_ = {};
	unique_memory<value_type> values_ = {};
	[[no_unique_address]] hasher hasher_ = {}; // usually a stateless functor
};

} // namespace util
