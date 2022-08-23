#pragma once

// Simple C++ wrapper for the HDF5 library
//     * easier to use than the C++ wrappers included in the HDF5 library
//       itself, essentially no boilerplate code required.
//     * exposes only a small subset of HDF5's features (no fancy indexing, no
//       customizable chunking, no compression, ...), just enough for my needs
//     * might (should?) migrate to some more serious c++ wrapper library. For
//       example either of these look promising:
//           github.com/BlueBrain/HighFive
//           github.com/steven-varga/h5cpp

#include "hdf5/serial/hdf5.h"
#include <cassert>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace util {

template <typename T> hid_t h5_type_id()
{
	if constexpr (std::is_same_v<T, float>)
		return H5T_NATIVE_FLOAT;
	else if constexpr (std::is_same_v<T, double>)
		return H5T_NATIVE_DOUBLE;
	else if constexpr (std::is_same_v<T, int8_t>)
		return H5T_NATIVE_INT8;
	else if constexpr (std::is_same_v<T, int16_t>)
		return H5T_NATIVE_INT16;
	else if constexpr (std::is_same_v<T, int32_t>)
		return H5T_NATIVE_INT32;
	else if constexpr (std::is_same_v<T, int64_t>)
		return H5T_NATIVE_INT64;
	else
		assert(false);
}

class Hdf5Dataset
{
	hid_t id_ = H5I_INVALID_HID;
	size_t size_ = 0;
	std::vector<hsize_t> shape_ = {};

  public:
	// move-only
	Hdf5Dataset(const Hdf5Dataset &) = delete;
	Hdf5Dataset &operator=(const Hdf5Dataset &) = delete;

	Hdf5Dataset(Hdf5Dataset &&f) noexcept
	    : id_(std::exchange(f.id_, H5I_INVALID_HID)),
	      size_(std::exchange(f.size_, 0)), shape_(f.shape_, {}){};
	Hdf5Dataset &operator=(Hdf5Dataset &&f) noexcept
	{
		close();
		id_ = std::exchange(f.id_, H5I_INVALID_HID);
		size_ = std::exchange(f.size_, 0);
		shape_ = std::exchange(f.shape_, {});
		return *this;
	}

	Hdf5Dataset() = default;
	explicit Hdf5Dataset(hid_t id);
	explicit operator bool() const { return id_ != H5I_INVALID_HID; }
	~Hdf5Dataset() { close(); }
	void close() noexcept
	{
		if (*this)
			if (H5Dclose(id_) < 0)
				assert(false);
		id_ = 0;
		size_ = 0;
		shape_ = {};
	}

	// size metrics
	size_t size() const { return size_; }
	std::vector<hsize_t> const &shape() { return shape_; }
	int rank() const { return (int)shape_.size(); }

	// read data ('row' always refers to the first index)

	void read_raw(hid_t type, void *data);
	void read_raw(hsize_t row, hid_t type, void *data);

	template <typename T> void read(std::span<T> data)
	{
		assert(data.size() == size());
		read_raw(h5_type_id<T>(), data.data());
	}
	template <typename T> void read(hsize_t row, std::span<T> data)
	{
		assert(shape_.size() >= 1 && data.size() == size() / shape_[0]);
		read_raw(row, h5_type_id<T>(), data.data());
	}

	template <typename T> std::vector<T> read()
	{
		auto r = std::vector<T>(size());
		read(r);
		return r;
	}

	// write data ('row' always refers to the first index)

	void write_raw(hid_t type, void const *data);
	void write_raw(hsize_t row, hid_t type, void const *data);

	template <typename T> void write(std::span<const T> data)
	{
		assert(data.size() == size());
		write_raw(h5_type_id<T>(), data.data());
	}
	template <typename T> void write(hsize_t row, std::span<const T> data)
	{
		assert(shape_.size() >= 1 && data.size() == size() / shape_[0]);
		write_raw(row, h5_type_id<T>(), data.data());
	}

	// workaround for failing template deduction
	template <typename T> void write(std::vector<T> const &data)
	{
		write<T>(std::span(data));
	}
	template <typename T> void write(hsize_t row, std::vector<T> const &data)
	{
		write<T>(row, std::span(data));
	}
};

class Hdf5File
{
	hid_t id_ = H5I_INVALID_HID;

	explicit Hdf5File(hid_t id) : id_(id) {}

	void set_attribute_raw(std::string const &, hid_t, const void *);
	void set_attribute_raw(std::string const &, hid_t, hsize_t, const void *);
	void get_attribute_raw(std::string const &name, hid_t type, void *data);

  public:
	// move-only
	Hdf5File(const Hdf5File &) = delete;
	Hdf5File &operator=(const Hdf5File &) = delete;

	Hdf5File(Hdf5File &&f) noexcept : id_(std::exchange(f.id_, H5I_INVALID_HID))
	{}
	Hdf5File &operator=(Hdf5File &&f) noexcept
	{
		close();
		id_ = std::exchange(f.id_, H5I_INVALID_HID);
		return *this;
	}

	// open / close
	Hdf5File() = default;
	~Hdf5File() { close(); }
	static Hdf5File create(std::string const &filename, bool overwrite = false);
	static Hdf5File open(std::string const &filename, bool writeable = false);
	explicit operator bool() const { return id_ != H5I_INVALID_HID; }
	void close() noexcept
	{
		if (*this)
			if (H5Fclose(id_) < 0)
				assert(false);
	}

	/** general object access */
	bool exists(std::string const &name);
	void remove(std::string const &name);

	/** access to datasets */
	Hdf5Dataset create_data(std::string const &name,
	                        const std::vector<hsize_t> &size,
	                        hid_t type = H5T_NATIVE_DOUBLE);
	Hdf5Dataset open_data(std::string const &name);

	// shortcut for open + reading a dataset
	template <typename T> std::vector<T> read_data(std::string const &name)
	{
		return open_data(name).read<T>();
	}

	/** shortcut for creating + writing dataset */
	template <typename T>
	Hdf5Dataset write_data(std::string const &name, std::vector<T> const &data)
	{
		auto ds = create_data(name, {data.size()}, h5_type_id<T>());
		ds.write(data);
		return ds;
	}

	/** groups */
	void make_group(std::string const &name);

	/** attributes */

	bool has_attribute(std::string const &name);

	void set_attribute(std::string const &name, double v);
	void set_attribute(std::string const &name, int v);
	void set_attribute(std::string const &name, std::string const &v);
	void set_attribute(std::string const &name, const std::vector<double> &v);
	void set_attribute(std::string const &name, const std::vector<int> &v);

	template <typename T, size_t N>
	void set_attribute(std::string const &name, const std::array<T, N> &v)
	{
		set_attribute(name, std::vector<T>(v.begin(), v.end()));
	}

	template <typename T>
	void set_attribute(std::string const &name, std::optional<T> const &v)
	{
		if (v)
			set_attribute(name, *v);
	}

	template <typename T> T get_attribute(std::string const &name);
	template <typename T> T get_attribute(std::string const &name, T const &def)
	{
		return get_optional_attribute<T>(name).value_or(def);
	}

	template <typename T>
	std::optional<T> get_optional_attribute(std::string const &name)
	{
		if (has_attribute(name))
			return get_attribute<T>(name);
		else
			return std::nullopt;
	}
};

} // namespace util
