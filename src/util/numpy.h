#pragma once

// read and write memory-mapped .npy files from python's numpy library

#include "util/json.h"
#include "util/memory.h"
#include "util/span.h"
#include <string>

namespace util {

template <typename T> constexpr std::string numpy_type()
{
	// assumes little-endian platform
	if constexpr (std::is_same_v<T, float>)
		return "<f4";
	else if constexpr (std::is_same_v<T, double>)
		return "<f8";
	else if constexpr (std::is_same_v<T, int8_t>)
		return "<i1";
	else if constexpr (std::is_same_v<T, int16_t>)
		return "<i4";
	else if constexpr (std::is_same_v<T, int32_t>)
		return "<i8";
	else
		assert(false);
}

template <class T> class NumpyFile
{
	MappedFile file_;
	T *data_ = nullptr; // points into memory-mapped file
	size_t size_ = 0;   // flat size = product(shape)
	std::vector<size_t> shape_ = {};

	static constexpr char magic[8] = {char(0x93), 'N', 'U', 'M',
	                                  'P',        'Y', 1,   0};

  public:
	NumpyFile() = default;
	~NumpyFile() = default;

	NumpyFile(NumpyFile &&other) noexcept
	    : file_(std::exchange(other.file_, {})),
	      data_(std::exchange(other.data_, nullptr)),
	      size_(std::exchange(other.size_, 0)),
	      shape_(std::exchange(other.shape_, {}))
	{}

	NumpyFile &operator=(NumpyFile &&other) noexcept
	{
		file_ = std::exchange(other.file_, {});
		data_ = std::exchange(other.data_, nullptr);
		size_ = std::exchange(other.size_, 0);
		shape_ = std::exchange(other.shape_, {});
		return *this;
	}

	static NumpyFile open(std::string const &filename, bool writeable = false)
	{
		auto file = MappedFile::open(filename, writeable);
		if (file.size() < sizeof(magic) + 2)
			throw std::runtime_error(
			    "could not open numpy file (file too short)");

		if (std::memcmp(file.data(), magic, sizeof(magic)))
			throw std::runtime_error("could not open numpy file (invalid "
			                         "header, or unsupported version)");
		size_t header_len = *(uint16_t *)((char *)file.data() + sizeof(magic));
		if ((header_len + sizeof(magic) + 2) % 64)
			throw std::runtime_error(
			    "could not open numpy file (invalid header size)");

		// the header is a "python literal expression", which is not json,
		// but our json parser is general enough to handle it.
		auto header_source = std::string_view(
		    (char *)((char *)file.data() + sizeof(magic) + 2), header_len);
		auto header = Json::parse(header_source);
		if (header["fortran_order"].get<bool>())
			throw std::runtime_error(
			    "could not open numpy file (fortran order)");
		auto descr = header["descr"].get<std::string>();
		if (descr != numpy_type<T>())
			throw std::runtime_error(fmt ::format(
			    "could not open numpy file (unexpected datatype '{}')", descr));
		auto shape = header["shape"].get<std::vector<size_t>>();
		size_t size = 1;
		for (auto s : shape)
			size *= s;
		if (file.size() < sizeof(magic) + 2 + header_len + sizeof(T) * size)
			throw std::runtime_error(
			    "could not open numpy file (data too short)");

		NumpyFile np;
		np.data_ = (T *)((char *)file.data() + header_len + sizeof(magic) + 2);
		np.file_ = std::move(file);
		np.size_ = size;
		np.shape_ = std::move(shape);
		return np;
	}

	// TODO: create()

	size_t size() const { return size_; }
	int rank() const { return (int)shape_.size(); }
	auto const &shape() const { return shape_; }

	T *data() { return data_; }
	T const *data() const { return data_; }

	util::span<T> flat() { return util::span<T>(data(), size()); }
	util::span<const T> flat() const { return util::span<T>(data(), size()); }

	template <size_t dim> util::ndspan<T, dim> view()
	{
		if (rank() != dim)
			throw std::runtime_error("numpy array dimension mismatch");
		typename util::ndspan<T, dim>::index_type s;
		std::copy(shape().begin(), shape().end(), s.begin());
		return util::ndspan<T, dim>(flat(), s);
	}
	template <size_t dim> util::ndspan<const T, dim> view() const
	{
		if (rank() != dim)
			throw std::runtime_error("numpy array dimension mismatch");
		typename util::ndspan<const T, dim>::index_type s;
		std::copy(shape().begin(), shape().end(), s.begin());
		return util::ndspan<const T, dim>(flat(), s);
	}
};

} // namespace util
