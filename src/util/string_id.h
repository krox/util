#pragma once

#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
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
 *   - Should really use a more efficient allocator for the storage. This is
 *     a perfect usecase for a simple bump allocator.
 *   - std::unordered_map is probably not a perfect choice of container here
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
	int id() const { return id_; }
};

bool operator==(string_id a, string_id b) { return a.id() == b.id(); }
bool operator!=(string_id a, string_id b) { return a.id() != b.id(); }

class StringPool
{
	// very crude datastructures, causing a lot more fragmentation and
	// allocation overhead than neccessary...
	std::unordered_map<std::string_view, string_id> lookup_;
	std::vector<std::string> table_;

  public:
	StringPool()
	{
		table_.emplace_back("");
		lookup_[table_[0]] = string_id(0);
	}

	// convert string->id, either looking up an existing id, or making a new one
	string_id id(std::string_view s)
	{
		if (auto it = lookup_.find(s); it != lookup_.end())
			return it->second;
		auto r = string_id((int)table_.size());
		table_.emplace_back(s);
		lookup_[table_.back()] = r;
		return r;
	}

	// convert id->string
	std::string_view str(string_id i) const { return table_[i.id()]; }
	char const *c_str(string_id i) const { return table_[i.id()].c_str(); }

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
