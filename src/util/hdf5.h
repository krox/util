#pragma once

/**
 * Simple C++ wrapper for HDF5.
 */

#include "hdf5/serial/hdf5.h"
#include "util/span.h"
#include <optional>
#include <string>

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
	else
		assert(false);
}

class DataSet
{
	hid_t id = 0;

  public:
	size_t size;
	std::vector<hsize_t> shape;
	int rank() const { return (int)shape.size(); }

	/** non copyable but movable */
	DataSet(const DataSet &) = delete;
	DataSet &operator=(const DataSet &) = delete;
	DataSet(DataSet &&f) : id(f.id) { f.id = 0; };
	DataSet &operator=(DataSet &&f)
	{
		close();
		id = f.id;
		size = f.size;
		shape = f.shape;
		f.id = 0;
		return *this;
	}

	DataSet() = default;
	explicit DataSet(hid_t id);
	~DataSet();
	void close();

	template <typename T> void write(std::span<const T> data);
	template <typename T> void write(hsize_t row, std::span<const T> data);

	// workaround for failing template deduction
	template <typename T> void write(std::vector<T> const &data)
	{
		write<T>(std::span<const T>(data));
	}
	template <typename T> void write(hsize_t row, std::vector<T> const &data)
	{
		write<T>(row, std::span<const T>(data));
	}

	template <typename T> void read(std::span<T> data);
	template <typename T> void read(hsize_t row, std::span<T> data);
	template <typename T> std::vector<T> read()
	{
		auto r = std::vector<T>(size);
		read(r);
		return r;
	}
};

class DataFile
{
	hid_t id = 0; // >0 for actually opened files

	explicit DataFile(hid_t id) : id(id) {}

	void setAttribute(const std::string &, hid_t, const void *);
	void setAttribute(const std::string &, hid_t, hsize_t, const void *);

	void getAttribute(const std::string &name, hid_t type, void *data);

  public:
	/** non copyable but movable */
	DataFile(const DataFile &) = delete;
	DataFile &operator=(const DataFile &) = delete;
	DataFile(DataFile &&f) : id(f.id) { f.id = 0; };
	DataFile &operator=(DataFile &&f)
	{
		close();
		id = f.id;
		f.id = 0;
		return *this;
	}

	/** open/close */
	DataFile() = default;
	~DataFile();
	static DataFile create(const std::string &filename, bool overwrite = false);
	static DataFile open(const std::string &filename, bool writeable = false);
	void close();

	explicit operator bool() const { return id != 0; }

	/** general object access */
	bool exists(const std::string &name);
	void remove(const std::string &name);

	/** access to datasets */
	DataSet createData(const std::string &name,
	                   const std::vector<hsize_t> &size,
	                   hid_t type = H5T_NATIVE_DOUBLE);
	DataSet openData(const std::string &name);

	/** shortcut for creating + writing dataset */
	template <typename T>
	DataSet writeData(const std::string &name, std::vector<T> const &data);

	/** groups */
	void makeGroup(const std::string &name);

	/** attributes */

	bool hasAttribute(const std::string &name);

	void setAttribute(const std::string &name, double v);
	void setAttribute(const std::string &name, int v);
	void setAttribute(const std::string &name, const std::string &v);
	void setAttribute(const std::string &name, const std::vector<double> &v);
	void setAttribute(const std::string &name, const std::vector<int> &v);

	template <typename T, size_t N>
	void setAttribute(const std::string &name, const std::array<T, N> &v)
	{
		setAttribute(name, std::vector<T>(v.begin(), v.end()));
	}

	template <typename T>
	void setAttribute(const std::string &name, std::optional<T> const &v)
	{
		if (v)
			setAttribute(name, *v);
	}

	template <typename T> T getAttribute(const std::string &name);
	template <typename T> T getAttribute(const std::string &name, T const &def)
	{
		return getOptionalAttribute<T>(name).value_or(def);
	}

	template <typename T>
	std::optional<T> getOptionalAttribute(const std::string &name)
	{
		if (hasAttribute(name))
			return getAttribute<T>(name);
		else
			return std::nullopt;
	}
};

} // namespace util
