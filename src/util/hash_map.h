#pragma once

/**
 * Associative map implemented as hash table
 * (open adressing / closed hashing)
 *
 * compared to std::unordered_map
 *     - Does not guarantee any pointer or iterator stability accross rehashing,
 *       which can happen at any insertion (no guaranteed capacity)
 *     - Does not proide strong exception guarantees (if the value type can
 *       throw on move, or allocation fails, things are really broken)
 *     - Implemented with closed hashing and linear probing. This produces a
 *       higher number of collisions, but is good for cache locality and avoids
 *       memory allocation overhead and defragmentation.
 *     - by default uses util::hash instead of std::hash
 *
 * TODO (maybe):
 *     - max_probe_length should be dynamic around O(log(n))
 *     - use SIMD for checking the control bytes (this would be important in
 *       order to actually beat std::unordered_map for efficiency)
 *     - allocate max_probe_length additional slots in the end, so no more
 *       wrap-around necessary
 *     - robin-hood hashing sounds good to remove the need for tombstones
 *     - a lot of functions are missing compared to std::unordered_map
 *           * emplace, try_emplace
 *           * overloads for heterogeneous lookup
 *           * some overloads taking Key&&
 *           * insert_or_assign()
 *           * ...
 *     - check excepetion safety. pretty sure everything is broken if the copy-
 *       (or move-) constructor of the value_type ever fails.
 *
 * implementation details (might change in the future):
 *     - 7-bit hashes per entry are stored for fast linear scanning
 *       (inspired by abseil::flat_hash_map)
 *     - full hash is not stored, thus has to be recomputed on rehashes
 *     - internal capacity is always a power-of-two (with a good default hash,
 *       this should not lead to an increased number of collisions)
 */

#include "util/hash.h"
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
	using key_type = Key;
	using mapped_type = T;
	using value_type = std::pair<const Key, T>;
	using hasher = Hash;

	class iterator : public std::iterator<std::forward_iterator_tag, value_type>
	{
		hash_map *map_ = nullptr;
		size_t index_ = 0;

	  public:
		iterator(hash_map *map, size_t index) : map_(map), index_(index) {}
		value_type &operator*() const { return map_->values_[index_]; }
		value_type *operator->() const { return &map_->values_[index_]; }
		bool operator!=(iterator other) const { return index_ != other.index_; }
		iterator &operator++()
		{
			++index_;
			while (index_ < map_->capacity_ && !(map_->control_[index_] & 128))
				++index_;
			return *this;
		}
	};

	class const_iterator
	    : public std::iterator<std::forward_iterator_tag, const value_type>
	{
		hash_map const *map_ = nullptr;
		size_t index_ = 0;

	  public:
		const_iterator(hash_map const *map, size_t index)
		    : map_(map), index_(index)
		{}
		value_type const &operator*() const { return map_->values_[index_]; }
		value_type const *operator->() const { return &map_->values_[index_]; }
		bool operator!=(const_iterator other) const
		{
			return index_ != other.index_;
		}
		const_iterator &operator++()
		{
			++index_;
			while (index_ < map_->capacity_ && !(map_->control_[index_] & 128))
				++index_;
			return *this;
		}
	};

	hash_map() noexcept : hasher_(){};

	explicit hash_map(Hash const &h) : hasher_(h) {}

	template <typename It>
	hash_map(It first, It last, Hash const &h = Hash()) : hasher_(h)
	{
		insert(first, last);
	}

	hash_map(std::initializer_list<value_type> ilist, Hash const &h = Hash())
	    : hasher_(h)
	{
		insert(ilist.begin(), ilist.end());
	}

	hash_map(hash_map const &other) : hasher_(other.hasher_)
	{
		// We just copy everything over, including tombstones.
		// Alternatively, we could clean out tombstones, but that would
		// require rehashing of everything. Not sure whats best here.

		size_ = other.size_;
		capacity_ = other.capacity_;
		control_ = std::malloc(capacity_);
		values_ = std::aligned_alloc(alignof(value_type),
		                             sizeof(value_type) * capacity_);

		std::memcpy(control_, other.control_, capacity_);
		for (size_t i = 0; i < capacity_; ++i)
			if (control_[i] & 128)
				new (&values_[i])(value_type)(other.values_[i]);
	}

	hash_map(hash_map &&other) noexcept : hasher(other.hasher_)
	{
		size_ = other.size_;
		capacity_ = other.capacity_;
		control_ = other.control_;
		values_ = other.values_;
		other.size_ = 0;
		other.capacity_ = 0;
		other.control_ = nullptr;
		other.values_ = nullptr;
	}

	hash_map &operator=(hash_map const &other)
	{
		hasher_ = other.hasher_;
		clear();
		insert(other.begin(), other.end());
		return *this;
	}

	hash_map &operator=(hash_map &&other) noexcept
	{
		hasher_ = other.hasher_;
		swap(*this, other);
		return *this;
	}

	hash_map &operator=(std::initializer_list<value_type> ilist)
	{
		clear();
		insert(ilist.begin(), ilist.end());
		return *this;
	}

	~hash_map()
	{
		clear();
		std::free(control_);
		std::free(values_);
	}

	iterator begin() noexcept
	{
		size_t i = 0;
		while (i < capacity_ && !(control_[i] & 128))
			++i;
		return iterator(this, i);
	}
	const_iterator begin() const noexcept
	{
		size_t i = 0;
		while (i < capacity_ && !(control_[i] & 128))
			++i;
		return const_iterator(this, i);
	}
	const_iterator cbegin() const noexcept { return begin(); }

	iterator end() noexcept { return iterator(this, capacity_); }
	const_iterator end() const noexcept
	{
		return const_iterator(this, capacity_);
	}
	const_iterator cend() const noexcept { return end(); }

	bool empty() const noexcept { return size_ == 0; }
	size_t size() const noexcept { return size_; };

	void clear() noexcept
	{
		for (size_t i = 0; i < capacity_; ++i)
		{
			if (control_[i] & 128)
				values_[i].~value_type();
			control_[i] = Empty; // also clear tombstones!
		}
		size_ = 0;
	}

	bool contains(Key const &key) const { return find_pos(key) != (size_t)-1; }
	size_t count(Key const &key) const { return contains(key); }
	iterator find(Key const &key)
	{
		if (auto i = find_pos(key); i != (size_t)-1)
			return iterator(this, i);
		else
			return end();
	}
	const_iterator find(Key const &key) const
	{
		if (auto i = find_pos(key); i != (size_t)-1)
			return const_iterator(this, i);
		else
			return end();
	}

	std::pair<iterator, bool> insert(value_type const &value)
	{
		auto [base, control] = hash(value.first);
		if (size_t pos = find_pos(value.first, base, control);
		    pos != (size_t)-1)
			return {{this, pos}, false};

		if (capacity_ == 0)
			rehash();
		while (true)
		{
			for (size_t i = 0; i < max_probe_length; ++i)
				if (auto pos = (base + i) & (capacity_ - 1);
				    (control_[pos] & 128) == 0)
				{
					control_[pos] = control;
					new (&values_[pos])(value_type)(value);
					++size_;
					return {{this, pos}, true};
				}
			rehash();
		}
	}

	std::pair<iterator, bool> insert(value_type &&value)
	{
		auto [base, control] = hash(value.first);
		if (size_t pos = find_pos(value.first, base, control);
		    pos != (size_t)-1)
			return {iterator(this, pos), false};

		if (capacity_ == 0)
			rehash();
		while (true)
		{
			assert(capacity_);
			for (size_t i = 0; i < max_probe_length; ++i)
				if (auto pos = (base + i) & (capacity_ - 1);
				    (control_[pos] & 128) == 0)
				{
					control_[pos] = control;
					new (&values_[pos])(value_type)(std::move(value));
					++size_;
					return {iterator(this, pos), true};
				}
			rehash();
		}
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

	size_t erase(Key const &k)
	{
		auto pos = find_pos(k);
		if (pos == (size_t)-1)
			return 0;

		// optimization: if the very next slot is empty (not tombstone), then
		// no (further) collision on the current position can have occured,
		// so it is save to not place a tombstone here either.
		if (control_[(pos + 1) & (capacity_ - 1)] == Empty)
			control_[pos] = Empty;
		else
			control_[pos] = Tombstone;
		values_[pos].~value_type();
		--size_;
		return 1;
	}

	T &at(Key const &key)
	{
		auto pos = find_pos(key);
		if (pos == (size_t)-1)
			throw std::out_of_range("util::hash_map::at(): key not found");
		return values_[pos].second();
	}
	T const &at(Key const &key) const
	{
		auto pos = find_pos(key);
		if (pos == (size_t)-1)
			throw std::out_of_range("util::hash_map::at(): key not found");
		return values_[pos].second();
	}

	T &operator[](Key const &key)
	{
		if (auto pos = find_pos(key); pos != (size_t)-1)
			return values_[pos].second;
		return insert({key, T()}).first->second;
	}

	friend void swap(hash_map &a, hash_map &b) noexcept
	{
		using std::swap;
		swap(a.size_, b.size_);
		swap(a.capacity_, b.capacity_);
		swap(a.contol_, b.control_);
		swap(a.values_, b.values_);
		swap(a.hasher_, b.hasher_);
	}

  private:
	// find internal position, returns -1 if not found
	size_t find_pos(Key const &key, size_t base, uint8_t control) const
	{
		if (capacity_ == 0)
			return (size_t)-1;

		for (size_t i = 0; i < max_probe_length; ++i)
			if (control_[(base + i) & (capacity_ - 1)] == control)
				if (values_[(base + i) & (capacity_ - 1)].first == key)
					return (base + i) & (capacity_ - 1);
		return (size_t)-1;
	}

	size_t find_pos(Key const &key) const
	{
		auto [base, control] = hash(key);
		return find_pos(key, base, control);
	}

	// rehash everything and double the size of table
	void rehash()
	{
		// Throwing an exception is preferrable to segfaulting on a (very) bad
		// hash function. Especially since we are allowed to throw (bad_alloc)
		// here anyway.
		if (capacity_ > std::max(size_ * 16, (size_t)1024))
			throw std::runtime_error("Too many collisions in util::hash_map. "
			                         "Probably the hash function is broken.");

		auto capacity_old = capacity_;
		auto control_old = control_;
		auto values_old = values_;

		capacity_ = capacity_ ? 2 * capacity_ : 16;

		control_ = (uint8_t *)std::calloc(capacity_, 1);
		values_ = (value_type *)std::aligned_alloc(
		    alignof(value_type), sizeof(value_type) * capacity_);

		size_ = 0;
		for (size_t i = 0; i < capacity_old; ++i)
			if (control_old[i] & 128)
			{
				insert(std::move(values_old[i]));
				values_old[i].~value_type();
			}

		std::free(control_old);
		std::free(values_old);
	}

	std::pair<uint64_t, uint8_t> hash(Key const &k) const noexcept
	{
		uint64_t h = hasher_(k);

		// Split into position and control byte.
		// Need to make sure the control byte does not collide with the 'Empty'
		// or 'Tombstone' values. The '|128' is the fastest way to do this,
		// though it wastes a little bit of power of the control byte.
		return {h >> 8, (h & 255) | 128};
	}

	// * rehashing occurs whenever this length is exceeded during insertion
	// * unsuccessful search might probe the full length if there are a lot
	//   of tombstones
	static constexpr size_t max_probe_length = 16;

	static constexpr uint8_t Empty = 0;
	static constexpr uint8_t Tombstone = 1;

	size_t size_ = 0;
	size_t capacity_ = 0;
	uint8_t *control_ = nullptr;
	value_type *values_ = nullptr;
	[[no_unique_address]] hasher hasher_; // usually a stateless functor
};

} // namespace util
