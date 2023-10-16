#include "util/io.h"

#include "fmt/format.h"
#include <cassert>
#include <cstdio>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace util {

void fclose_delete::operator()(FILE *p)
{
	if (std::fclose(p))
		assert(false);
}

FilePointer open_file(std::string_view filename, char const *mode)
{
	auto p = std::fopen(std::string(filename).c_str(), mode);
	if (!p)
		throw std::runtime_error(fmt::format("could not open file '{}' ('{}')",
		                                     filename, strerror(errno)));
	return FilePointer(p);
}

MappedFile::MappedFile(char const *filename, char const *mode, bool writeable)
{
	// open file
	auto file = open_file(filename, mode);
	int fd = fileno(file.get());

	// get file size (already open, so std::filesystem::file_size seems stupid)
	struct stat st;
	if (fstat(fd, &st))
		assert(false);
	size_ = st.st_size;

	// mmap() does not like zero length
	if (!size_)
		return;

	// create the mapping
	auto prot = PROT_READ;
	if (writeable)
		prot |= PROT_WRITE;
	int flags = MAP_SHARED;
	ptr_ = mmap(nullptr, size_, prot, flags, fd, 0);
	file.reset();

	// check for errors
	if (ptr_ == MAP_FAILED)
	{
		ptr_ = nullptr;
		size_ = 0;
		throw std::runtime_error(
		    fmt::format("could not mmap() file '{}' (errno = {})", filename,
		                strerror(errno)));
	}
}

MappedFile MappedFile::open(std::string_view filename, bool writeable)
{
	return MappedFile(std::string(filename).c_str(), writeable ? "r+" : "r",
	                  writeable);
}

MappedFile MappedFile::create(std::string_view filename, size_t size,
                              bool overwrite)
{
	auto file =
	    open_file(std::string(filename).c_str(), overwrite ? "w+" : "w+x");
	if (ftruncate(fileno(file.get()), size))
		assert(false);
	file.reset();
	return open(filename, true);
}

void MappedFile::close() noexcept
{
	assert(bool(size_) == bool(ptr_));
	if (size_)
		munmap(ptr_, size_);
	size_ = 0;
	ptr_ = nullptr;
}

std::string read_file(std::string_view filename)
{
	auto file = open_file(filename, "rb");

	std::fseek(file.get(), 0u, SEEK_END);
	const auto size = std::ftell(file.get());
	std::fseek(file.get(), 0u, SEEK_SET);

	std::string s;
	s.resize(size);

	auto read = std::fread(&s[0], 1u, size, file.get());
	assert((size_t)read == (size_t)size);

	return s;
}

} // namespace util