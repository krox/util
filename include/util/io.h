#pragma once

#include "util/memory.h"
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// file IO utilities

namespace util {

// class for reading/writing (binary) files
class File
{
	FILE *file_ = nullptr;

  public:
	File() = default;

	// destructor and move semantics (move-only type)
	~File() noexcept { close(); }
	File(File &&other) noexcept : file_(std::exchange(other.file_, nullptr)) {}
	File &operator=(File &&other) noexcept
	{
		if (this == &other)
			return *this;
		close();
		file_ = std::exchange(other.file_, nullptr);
		return *this;
	}
	File(File const &) = delete;
	File &operator=(File const &) = delete;

	static File open(std::string_view file, bool writeable = false);
	static File create(std::string_view file, bool overwrite = false);
	void close() noexcept;

	explicit operator bool() const { return file_ != nullptr; }

	// get raw FILE* pointer for low-level operations
	FILE *get() const { return file_; }

	// flush internal buffer
	//   * does not guarantee disk write (due to buffering in OS)
	void flush();

	// move position in file
	void seek(size_t pos);
	void skip(size_t bytes);
	size_t tell() const;

	// read/write 'size' bytes from/to file
	void read_raw(void *buffer, size_t size);
	void write_raw(void const *buffer, size_t size);

	// read write a single value of a simple type T
	//     * 'trivially_copyable' ensures the type can be 'copied by memcpy'
	//     * there is still plenty of room for platform-dependence (e.g.,
	//       alignment, endianess) and wrong semantics (e.g. writing a pointer),
	//       so be careful.
	template <class T>
	    requires std::is_trivially_copyable_v<T>
	void read(T &value)
	{
		read_raw(&value, sizeof(T));
	}
	template <class T>
	    requires std::is_trivially_copyable_v<T>
	void write(T const &value)
	{
		write_raw(&value, sizeof(T));
	}

	// read/write multiple values of a simple type T
	template <class T>
	    requires std::is_trivially_copyable_v<T>
	void read(T *data, size_t count)
	{
		read_raw(data, count * sizeof(T));
	}
	template <class T>
	    requires std::is_trivially_copyable_v<T>
	void write(T const *data, size_t count)
	{
		write_raw(data, count * sizeof(T));
	}

	// get underlying file descriptor (-1 if file is not open)
	int fd() const;

	// truncate file to the given size
	void truncate(size_t size);
};

class MappedFile
{
	void *ptr_ = nullptr;
	size_t size_ = 0;
	MappedFile(char const *, bool);

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

// zstd compression/decompression
std::string decompress(std::span<const std::byte> data);
std::vector<std::byte> compress(std::string_view text,
                                int compression_level = 3);

// convenience functions for reading/writing entire files
//   * reading a text file automatically detects and unpacks zstd compression.
//     Reading binary files does not do this.
std::string read_file(std::string_view filename);
std::vector<std::byte> read_binary_file(std::string_view filename);
void write_file(std::string_view filename, std::string_view data);

} // namespace util
