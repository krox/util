#include "util/memory.h"

#include "fmt/format.h"
#include <cassert>
#include <cstdio>
#include <sys/mman.h>
#include <sys/stat.h>

namespace util {

void *util_mmap(size_t length)
{
	if (length == 0)
		return nullptr;

	// * 'MAP_NORESERVE' enables 'allocating' more space than physically exists
	//   (including swap). SIGSEGV will be triggered on write if we run out.
	// * everything will be zero-initilized (due to security issues)
	int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
	int prot = PROT_READ | PROT_WRITE;
	auto p = mmap(nullptr, length, prot, flags, 0, 0);
	if (p == MAP_FAILED)
		throw std::bad_alloc{};
	return p;
}

void util_munmap(void *p, size_t length) noexcept
{
	if (length)
		if (munmap(p, length) != 0)
			assert(false);
}

MappedFile::MappedFile(char const *filename, char const *mode, bool writeable)
{
	// open file
	auto file = std::fopen(filename, mode);
	if (!file)
		throw std::runtime_error(
		    fmt::format("could not open file '{}'", filename));
	int fd = fileno(file);

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
	if (std::fclose(file))
		assert(false);

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

MappedFile MappedFile::open(std::string const &filename, bool writeable)
{
	return MappedFile(filename.c_str(), writeable ? "r+" : "r", writeable);
}

MappedFile MappedFile ::create(std::string const &filename, bool overwrite)
{
	return MappedFile(filename.c_str(), overwrite ? "w+" : "w+x", true);
}

void MappedFile::close() noexcept
{
	assert(bool(size_) == bool(ptr_));
	if (size_)
		munmap(ptr_, size_);
	size_ = 0;
	ptr_ = nullptr;
}

} // namespace util
