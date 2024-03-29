#pragma once

#include "util/hash_map.h"
#include <compare>
#include <cstring>
#include <map>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

/**
 * Very simple implementation of "string interning", i.e., replacing
 * (immutable) strings with integers indexing into a (global) storage. This
 * makes passing around and equality-comparing such 'strings' extremely
 * efficienty.
 *
 *   - It is expected that the user creates a more or less global instance
 *     of StringPool, though this module does not provide one.
 *   - The empty string is always represented by 0 and all other strings are
 *     represented by consecutive positve integers starting at 1.
 *   - string_id only support equality comparisons, not a full ordering.
 *     This avoids confusion between "a.id() < b.id()" (which is fast but mostly
 *     meaningless) and "store.str(a) < store.str(b)" (which is slow but
 *     compatible with the usual comparion of string/string_view)
 *   - hash<string_id> returns the id, which is perfect in the sense that
 *     there cant be any collisions. But this is of course not compatible with
 *     hash<string> or hash<string_view>.
 *
 * TODO:
 *   - A std::string_view takes 16 bytes, which is larger than most strings
 *     in the usecases I have in mind, so some small-buffer-optimization
 *     seems very reasonable here in order to minimize memory usage.
 *   - Multi-threading support should be possible in a way that does not
 *     require any locking at all on lookups.
 */

namespace util {

struct string_id
{
	// could make the type of this configurable (by template paramter)
	int16_t id_ = 0;

	string_id() = default;
	explicit string_id(int i) : id_((int16_t)i)
	{
		assert(0 <= i && i <= INT16_MAX);
	}
	int id() const noexcept { return id_; }

	// empty string is always represented by zero, so this is meaningful
	// independent of the pool.
	explicit operator bool() const noexcept { return id_; }
};

bool operator==(string_id a, string_id b) noexcept { return a.id() == b.id(); }
std::strong_ordering operator<=>(string_id a, string_id b) noexcept
{
	return a.id() <=> b.id();
}

class StringPool
{
	// very crude datastructures, causing a lot more fragmentation and
	// allocation overhead than neccessary...
	util::hash_map<std::string_view, string_id> lookup_;
	std::vector<std::string_view> table_;

	std::pmr::monotonic_buffer_resource memory_pool_;
	std::pmr::polymorphic_allocator<char> alloc_{&memory_pool_};

  public:
	StringPool()
	{
		auto empty_id = id("");
		assert(empty_id.id() == 0);
	}

	// convert string->id, either looking up an existing id, or making a new one
	string_id id(std::string_view s)
	{
		if (auto it = lookup_.find(s); it != lookup_.end())
			return it->second;
		if (table_.size() == UINT16_MAX)
			throw std::runtime_error("StringPool overflow");

		// copy string data to internal storage (adding 0-terminator)
		char *p = alloc_.allocate(s.size() + 1);
		std::memcpy(p, s.data(), s.size());
		p[s.size()] = '\0';

		// add internalized string to tables
		auto r = string_id((int)table_.size());
		table_.emplace_back(p, s.size());
		lookup_[table_.back()] = r;
		return r;
	}

	// convert id->string
	std::string_view str(string_id i) const { return table_[i.id()]; }
	char const *c_str(string_id i) const
	{
		// we guarantee all strings to already be stored with 0-terminator
		return table_[i.id()].data();
	}

	// overload the call operator to save some typing
	string_id operator()(std::string_view s) { return id(s); }
	std::string_view operator()(string_id i) const { return str(i); }
};

} // namespace util

namespace std {
template <> struct hash<util::string_id>
{
	size_t operator()(util::string_id const &i) const { return (size_t)i.id(); }
};
} // namespace std
