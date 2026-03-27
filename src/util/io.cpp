#include "util/io.h"

#include "fmt/format.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef UTIL_ZSTD
#include <zstd.h>
#endif

namespace util {

namespace {

constexpr std::byte ZSTD_FRAME_MAGIC[] = {std::byte{0x28}, std::byte{0xB5},
                                          std::byte{0x2F}, std::byte{0xFD}};

bool has_zstd_magic(std::span<const std::byte> data)
{
	if (data.size() < sizeof(ZSTD_FRAME_MAGIC))
		return false;
	return std::memcmp(data.data(), ZSTD_FRAME_MAGIC,
	                   sizeof(ZSTD_FRAME_MAGIC)) == 0;
}

} // namespace

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

File File::open(std::string_view filename, bool writeable)
{
	File file;
	file.file_ = open_file(filename, writeable ? "r+" : "r");
	return file;
}

File File::create(std::string_view filename, bool overwrite)
{
	File file;
	file.file_ = open_file(filename, overwrite ? "w+" : "w+x");
	return file;
}

void File::close() noexcept { file_.reset(); }

void File::flush()
{
	assert(file_);
	if (std::fflush(file_.get()))
		throw std::runtime_error("could not flush file");
}

void File::seek(size_t pos)
{
	assert(file_);
	if (std::fseek(file_.get(), pos, SEEK_SET))
		throw std::runtime_error("could not seek in file");
}

void File::skip(size_t bytes)
{
	assert(file_);
	if (std::fseek(file_.get(), bytes, SEEK_CUR))
		throw std::runtime_error("could not seek in file");
}

size_t File::tell() const
{
	assert(file_);
	auto pos = std::ftell(file_.get());
	if (pos == -1)
		throw std::runtime_error("could not tell position in file");
	return pos;
}

void File::read_raw(void *buffer, size_t size)
{
	assert(file_);
	auto r = std::fread(buffer, 1, size, file_.get());
	if (r != size)
		throw std::runtime_error("could not read from file");
}

void File::write_raw(void const *buffer, size_t size)
{
	assert(file_);
	auto r = std::fwrite(buffer, 1, size, file_.get());
	if (r != size)
		throw std::runtime_error("could not write to file");
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

std::string decompress(std::span<const std::byte> data)
{
#ifdef UTIL_ZSTD
	auto dctx = ZSTD_createDCtx();
	if (!dctx)
		throw std::runtime_error(
		    "zstd decompression failed: could not allocate dctx");

	std::string out;
	out.reserve(ZSTD_DStreamOutSize());

	std::vector<char> chunk(ZSTD_DStreamOutSize());
	ZSTD_inBuffer in = {data.data(), data.size(), 0};

	while (in.pos < in.size)
	{
		ZSTD_outBuffer out_buf = {chunk.data(), chunk.size(), 0};
		auto const ret = ZSTD_decompressStream(dctx, &out_buf, &in);
		if (ZSTD_isError(ret))
		{
			ZSTD_freeDCtx(dctx);
			throw std::runtime_error(fmt::format(
			    "zstd decompression failed: {}", ZSTD_getErrorName(ret)));
		}

		out.append(chunk.data(), out_buf.pos);
	}

	ZSTD_freeDCtx(dctx);
	return out;
#else
	(void)data;
	throw std::runtime_error(
	    "util built without zstd support (enable UTIL_ZSTD to decompress)");
#endif
}

std::vector<std::byte> compress(std::string_view text, int compression_level)
{
#ifdef UTIL_ZSTD
	auto const max_size = ZSTD_compressBound(text.size());
	std::vector<std::byte> out(max_size);
	auto const written = ZSTD_compress(out.data(), out.size(), text.data(),
	                                   text.size(), compression_level);
	if (ZSTD_isError(written))
		throw std::runtime_error(fmt::format("zstd compression failed: {}",
		                                     ZSTD_getErrorName(written)));
	out.resize(written);
	return out;
#else
	(void)text;
	(void)compression_level;
	throw std::runtime_error(
	    "util built without zstd support (enable UTIL_ZSTD to compress)");
#endif
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
	if ((size_t)read != (size_t)size)
		throw std::runtime_error("could not read from file");

	if (has_zstd_magic(
	        std::span(reinterpret_cast<const std::byte *>(s.data()), s.size())))
	{
		return decompress(
		    std::span(reinterpret_cast<const std::byte *>(s.data()), s.size()));
	}

	return s;
}

std::vector<std::byte> read_binary_file(std::string_view filename)
{
	static_assert(sizeof(std::byte) == 1);

	auto file = open_file(filename, "rb");

	std::fseek(file.get(), 0u, SEEK_END);
	const auto size = std::ftell(file.get());
	std::fseek(file.get(), 0u, SEEK_SET);

	std::vector<std::byte> s;
	s.resize(size);

	auto read = std::fread(s.data(), 1u, size, file.get());
	if ((size_t)read != (size_t)size)
		throw std::runtime_error("could not read from file");

	return s;
}

void write_file(std::string_view filename, std::string_view data)
{
	assert(!filename.empty());
	auto file = open_file(filename, "wb");

	auto written = std::fwrite(data.data(), 1u, data.size(), file.get());
	if ((size_t)written != (size_t)data.size())
		throw std::runtime_error("could not write to file");
}

} // namespace util