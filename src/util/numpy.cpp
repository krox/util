#include "util/numpy.h"

#include "fmt/ranges.h"
#include "util/io.h"
#include <algorithm>
#include <numeric>

namespace util {

// for example "<f8" -> 8
size_t numpy_type_size(std::string const &dtype)
{
	if (dtype == "<i1")
		return 1;
	if (dtype == "<i2")
		return 2;
	if (dtype == "<i4")
		return 4;
	if (dtype == "<i8")
		return 8;
	if (dtype == "<f4")
		return 4;
	if (dtype == "<f8")
		return 8;
	if (dtype == "<c8")
		return 8;
	if (dtype == "<c16")
		return 16;
	throw std::runtime_error(fmt::format("unknown numpy dtype '{}'", dtype));
}

// first 8 bytes of an numpy file (v1.0 of the .npy format)
static constexpr char numpy_magic[8] = {char(0x93), 'N', 'U', 'M',
                                        'P',        'Y', 1,   0};

NumpyFile NumpyFile::open(std::string const &filename, bool writeable)
{
	auto file = MappedFile::open(filename, writeable);

	if (file.size() < 10)
		throw std::runtime_error("corrupt numpy file (too short for header)");
	if (std::memcmp(file.data(), numpy_magic, 8))
		throw std::runtime_error("could not open numpy file (invalid "
		                         "header, or unsupported version)");
	size_t header_len = *(uint16_t *)((char *)file.data() + 8);
	if ((10 + header_len) % 64 || file.size() < 10 + header_len)
		throw std::runtime_error("corrupt numpy file (invalid header size)");

	// the header is a "python literal expression", which is not json,
	// but our json parser is general enough to handle it.
	auto header_source = std::string_view((char *)file.data() + 10, header_len);
	auto header = Json::parse(header_source);
	if (header["fortran_order"].get<bool>())
		throw std::runtime_error("could not open numpy file (fortran order)");
	auto dtype = header["descr"].get<std::string>();
	size_t elem_size = numpy_type_size(dtype);
	auto shape = header["shape"].get<std::vector<size_t>>();
	size_t size = std::reduce(shape.begin(), shape.end(), 1, std::multiplies());
	if (file.size() < 10 + header_len + elem_size * size)
		throw std::runtime_error("corrupt numpy file (too short for data)");

	NumpyFile np;
	np.data_ = (char *)file.data() + header_len + 10;
	np.file_ = std::move(file);
	np.size_ = size;
	np.shape_ = std::move(shape);
	np.dtype_ = std::move(dtype);
	return np;
}

NumpyFile NumpyFile::create(std::string const &filename,
                            std::span<const size_t> shape,
                            std::string const &dtype, bool overwrite)
{
	// header size must be multiple of 64. Realistically, 128 is sufficient for
	// everything here (python only needs very long ones in case of complicated
	// compound dtype's, which we dont support here anyway)
	size_t size = std::reduce(shape.begin(), shape.end(), 1, std::multiplies());
	size_t filesize = 128 + numpy_type_size(dtype) * size;
	auto file = MappedFile::create(filename, filesize, overwrite);

	std::memcpy(file.data(), numpy_magic, 8);
	*(uint16_t *)((char *)file.data() + 8) = 118;
	std::memset((char *)file.data() + 10, ' ', 118);
	std::string header = fmt::format(
	    "{{'descr': '{}', 'fortran_order': False, 'shape': ({}), }}", dtype,
	    fmt::join(shape, ", "));
	assert(header.size() <= 118);
	std::memcpy((char *)file.data() + 10, header.data(), header.size());

	NumpyFile np;
	np.data_ = (char *)file.data() + 128;
	np.file_ = std::move(file);
	np.size_ = size;
	np.shape_ = std::vector<size_t>(shape.begin(), shape.end());
	np.dtype_ = dtype;
	return np;
}

} // namespace util