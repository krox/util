#pragma once

/**
 * Simple C++ wrapper for HDF5.
 */

#include "hdf5/serial/hdf5.h"
#include "util/span.h"
#include <string>

namespace util {

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

	void write(util::span<const double> data);
	void write(hsize_t row, util::span<const double> data);
	void read(util::span<double> data);
	template <typename T> std::vector<T> read();
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

	/** general object access */
	bool exists(const std::string &name);
	void remove(const std::string &name);

	/** access to datasets */
	DataSet createData(const std::string &name,
	                   const std::vector<hsize_t> &size);
	DataSet openData(const std::string &name);

	/** groups */
	void makeGroup(const std::string &name);

	/** attributes */
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

	template <typename T> T getAttribute(const std::string &name);
};

} // namespace util
