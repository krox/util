#pragma once

// read and write memory-mapped .npy files from python's numpy library

#include "util/complex.h"
#include "util/json.h"
#include "util/memory.h"
#include "util/span.h"
#include <complex>
#include <string>

namespace util {

template <typename T> std::string numpy_type()
{
	// assumes little-endian platform
	if constexpr (std::is_same_v<T, int8_t>)
		return "<i1";
	else if constexpr (std::is_same_v<T, int16_t>)
		return "<i2";
	else if constexpr (std::is_same_v<T, int32_t>)
		return "<i4";
	else if constexpr (std::is_same_v<T, int64_t>)
		return "<i8";
	else if constexpr (std::is_same_v<T, float>)
		return "<f4";
	else if constexpr (std::is_same_v<T, double>)
		return "<f8";
	else if constexpr (std::is_same_v<T, std::complex<float>> ||
	                   std::is_same_v<T, util::complex<float>>)
		return "<c8";
	else if constexpr (std::is_same_v<T, std::complex<double>> ||
	                   std::is_same_v<T, util::complex<double>>)
		return "<c16";
	else
		assert(false);
}

// for example "<f8" -> 8
size_t numpy_type_size(std::string const &dtype);

// memory-mapped .npy file
class NumpyFile
{
	MappedFile file_;
	void *data_ = nullptr; // points into memory-mapped file
	size_t size_ = 0;      // flat size = product(shape)
	std::vector<size_t> shape_ = {};
	std::string dtype_ = ""; // for example "<f8" for little-endian 'double'

  public:
	// special members
	NumpyFile() = default;
	~NumpyFile() { close(); }

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

	void close() noexcept
	{
		file_.close();
		data_ = nullptr;
		size_ = 0;
		shape_ = {};
		dtype_ = {};
	}

	explicit operator bool() const { return !!file_; }

	// pseudo-constructors

	static NumpyFile open(std::string const &filename, bool writeable = false);
	static NumpyFile create(std::string const &filename,
	                        std::span<const size_t> shape,
	                        std::string const &dtype = "<f8",
	                        bool overwrite = false);

	// size/type information

	std::string const &dtype() const { return dtype_; }
	size_t size() const { return size_; }
	size_t size_bytes() const { return size_ * numpy_type_size(dtype()); }
	int rank() const { return (int)shape_.size(); }
	auto const &shape() const { return shape_; }

	// untyped data access

	void *raw_data() { return data_; }
	void const *raw_data() const { return data_; }

	std::span<std::byte> raw_bytes()
	{
		return {static_cast<std::byte *>(raw_data()), size_bytes()};
	}
	std::span<const std::byte> raw_bytes() const
	{
		return {static_cast<std::byte const *>(raw_data()), size_bytes()};
	}

	// typed access (template type must match file data)
	template <class T> T *data()
	{
		if (numpy_type<T>() != dtype())
			throw std::runtime_error(fmt::format(
			    "type error in numpy file. expected '{}', got '{}'.",
			    numpy_type<T>(), dtype()));
		return static_cast<T *>(data_);
	}
	template <class T> T const *data() const
	{
		if (numpy_type<T>() != dtype())
			throw std::runtime_error(fmt::format(
			    "type error in numpy file. expected '{}', got '{}'.",
			    numpy_type<T>(), dtype()));
		return static_cast<T const *>(data_);
	}

	template <class T> std::span<T> flat()
	{
		return std::span(data<T>(), size());
	}
	template <class T> std::span<const T> flat() const
	{
		return std::span(data<T>(), size());
	}

	template <class T, size_t dim> util::ndspan<T, dim> view()
	{
		if (rank() != dim)
			throw std::runtime_error("numpy array dimension mismatch");
		typename util::ndspan<T, dim>::index_type s;
		std::copy(shape().begin(), shape().end(), s.begin());
		return util::ndspan<T, dim>(flat<T>(), s);
	}
	template <class T, size_t dim> util::ndspan<const T, dim> view() const
	{
		if (rank() != dim)
			throw std::runtime_error("numpy array dimension mismatch");
		typename util::ndspan<const T, dim>::index_type s;
		std::copy(shape().begin(), shape().end(), s.begin());
		return util::ndspan<const T, dim>(flat<T>(), s);
	}
};

} // namespace util
