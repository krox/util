#pragma once

#include "util/memory.h"
#include <string>
#include <string_view>
#include <vector>

// file IO utilities

namespace util {

// little wrapper for fopen() / close() that does RAII and throws on errors
struct fclose_delete
{
	void operator()(FILE *p);
};
using FilePointer = std::unique_ptr<FILE, fclose_delete>;
FilePointer open_file(std::string_view filename, char const *mode);

class MappedFile
{
	void *ptr_ = nullptr;
	size_t size_ = 0;
	MappedFile(char const *, char const *, bool);

  public:
	// constructors
	MappedFile() = default;

	static MappedFile open(std::string_view file, bool writeable = false);
	static MappedFile create(std::string_view file, size_t size,
	                         bool overwrite = false);
	void close() noexcept;

	// special members (move-only type)
	~MappedFile() { close(); };
	MappedFile(MappedFile &&other) noexcept
	    : ptr_(std::exchange(other.ptr_, nullptr)),
	      size_(std::exchange(other.size_, 0))
	{}
	MappedFile &operator=(MappedFile &&other) noexcept
	{
		close();
		ptr_ = std::exchange(other.ptr_, nullptr);
		size_ = std::exchange(other.size_, 0);
		return *this;
	}

	// data access
	// NOTE: if the file is opened as read-only, writing should be considered
	//       undefined behaviour, not sure about platform specifics
	void *data() { return ptr_; }
	void const *data() const { return ptr_; }
	size_t size() const { return size_; }
	explicit operator bool() const { return ptr_; }
};

std::string read_file(std::string_view filename);
std::vector<std::byte> read_binary_file(std::string_view filename);

} // namespace util
