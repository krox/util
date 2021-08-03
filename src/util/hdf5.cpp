
#include "util/hdf5.h"

#include <cassert>
#include <stdexcept>

namespace util {

namespace {
hid_t enforce(hid_t id)
{
	if (id < 0)
		throw std::runtime_error("HDF5 error");
	return id;
}

} // namespace

DataSet::DataSet(hid_t id) : id(id)
{
	if (id <= 0)
		return;
	auto space = enforce(H5Dget_space(id));
	size = H5Sget_simple_extent_npoints(space);
	shape.resize(64);
	auto rank = H5Sget_simple_extent_dims(space, &shape[0], nullptr);
	shape.resize(rank);
	H5Sclose(space);
}

void DataSet::close()
{
	if (id > 0)
		H5Dclose(id);
}

DataSet::~DataSet() { close(); }

void DataSet::read(util::span<double> data)
{
	assert(data.size() == size);
	enforce(H5Dread(id, H5T_NATIVE_DOUBLE, 0, 0, 0, data.data()));
}

template <> std::vector<double> DataSet::read<double>()
{
	auto r = std::vector<double>(size);
	read(r);
	return r;
}

template <typename T> void DataSet::write(util::span<const T> data)
{
	assert(data.size() == size);
	enforce(H5Dwrite(id, h5_type_id<T>(), 0, 0, 0, data.data()));
}

template <typename T> void DataSet::write(hsize_t row, util::span<const T> data)
{
	assert(id > 0);
	assert(row < shape[0]);
	assert(data.size() == size / shape[0]);

	auto offset = std::vector<hsize_t>(rank(), 0);
	offset[0] = row;
	auto memspace = enforce(H5Screate_simple(rank() - 1, &shape[1], nullptr));
	std::vector<hsize_t> rowShape = shape;
	rowShape[0] = 1;
	auto space = enforce(H5Dget_space(id));
	H5Sselect_hyperslab(space, H5S_SELECT_SET, offset.data(), nullptr,
	                    rowShape.data(), nullptr);

	enforce(H5Dwrite(id, h5_type_id<T>(), memspace, space, 0, data.data()));

	H5Sclose(memspace);
	H5Sclose(space);
}

DataFile::~DataFile() { close(); }

DataFile DataFile::create(const std::string &filename, bool overwrite)
{
	auto mode = overwrite ? H5F_ACC_TRUNC : H5F_ACC_EXCL;
	auto id = enforce(H5Fcreate(filename.c_str(), mode, 0, 0));
	return DataFile(id);
}

DataFile DataFile::open(const std::string &filename, bool writeable)
{
	auto mode = writeable ? H5F_ACC_RDWR : H5F_ACC_RDONLY;
	auto id = enforce(H5Fopen(filename.c_str(), mode, 0));
	return DataFile(id);
}

void DataFile::close()
{
	if (id > 0)
		H5Fclose(id);
	id = 0;
}

namespace {

/** guess a hopefully reasonable chunk size for a dataset */
std::vector<hsize_t> guessChunkSize(std::vector<hsize_t> const &size,
                                    hid_t type)
{
	//  - Auto-chunking in the h5py library tries to keep chunks somewhat
	//    close to square. This is reasonable if nothing is known about the
	//    dimensions and arbitrary slices might occur.
	//  - We instead keep the trailing dimension(s) contiguous, chunking only
	//    the leading one(s). This is (hopefully) reasonable if the user already
	//    optimized the order of dimenions for performance in "row major" order.

	assert(size.size() >= 1);
	auto typeSize = H5Tget_size(type);
	assert(typeSize > 0 && typeSize <= 1024);

	// HDF5 guideline is to keep chunk size between ~10KiB and 1MiB
	size_t minElems = 8 * 1024 / typeSize;    // soft limit
	size_t maxElems = 1024 * 1024 / typeSize; // hard limit

	// first guess: whole dataset as a single chunk
	//              (with size 1 on resizable dimensions)
	auto chunk = size;
	size_t elems = 1;
	bool resizable = false;
	for (auto &c : chunk)
	{
		if (c == 0) // indicates resizable dimension
		{
			c = 1;
			resizable = true;
		}
		elems *= c;
	}

	// chunk too small -> enlarge in resizable dimension(s)
	while (elems < minElems && resizable)
	{
		for (size_t i = 0; i < size.size(); ++i)
			if (size[i] == 0)
			{
				chunk[i] *= 2;
				elems *= 2;
			}
	}

	// chunk too large -> cut down in leading dimension(s)
	for (size_t i = 0; i < size.size() && elems > maxElems; ++i)
		while (chunk[i] > 1 && elems > maxElems)
		{
			elems /= chunk[i];
			chunk[i] /= 2;
			elems *= chunk[i];
		}
	return chunk;
}

} // namespace

DataSet DataFile::createData(const std::string &name,
                             const std::vector<hsize_t> &size, hid_t type)
{
	assert(id > 0);
	auto space = enforce(H5Screate_simple(size.size(), size.data(), nullptr));
	auto props = enforce(H5Pcreate(H5P_DATASET_CREATE));
	auto chunk = guessChunkSize(size, type);
	if (size.size() > 0)
	{
		H5Pset_chunk(props, chunk.size(), chunk.data());
		enforce(H5Pset_fletcher32(props));
	}
	auto set = enforce(H5Dcreate2(id, name.c_str(), type, space, 0, props, 0));
	H5Pclose(props);
	H5Sclose(space);
	return DataSet(set);
}

DataSet DataFile::openData(const std::string &name)
{
	assert(id > 0);
	auto set = enforce(H5Dopen2(id, name.c_str(), 0));
	return DataSet(set);
}

bool DataFile::exists(const std::string &name)
{
	assert(id > 0);
	return enforce(H5Lexists(id, name.c_str(), 0)) > 0;
}

void DataFile::remove(const std::string &name)
{
	assert(id > 0);
	enforce(H5Ldelete(id, name.c_str(), 0));
}

void DataFile::makeGroup(const std::string &name)
{
	assert(id > 0);
	auto group = enforce(H5Gcreate2(id, name.c_str(), 0, 0, 0));
	H5Gclose(group);
}

void DataFile::setAttribute(const std::string &name, hid_t type, const void *v)
{
	assert(id > 0);
	auto space = enforce(H5Screate(H5S_SCALAR));
	auto attr = enforce(H5Acreate2(id, name.c_str(), type, space, 0, 0));
	enforce(H5Awrite(attr, type, v));
	H5Aclose(attr);
	H5Sclose(space);
}

void DataFile::setAttribute(const std::string &name, hid_t type, hsize_t count,
                            const void *v)
{
	assert(id > 0);
	auto space = enforce(H5Screate_simple(1, &count, nullptr));
	auto attr = enforce(H5Acreate2(id, name.c_str(), type, space, 0, 0));
	enforce(H5Awrite(attr, type, v));
	H5Aclose(attr);
	H5Sclose(space);
}

void DataFile::setAttribute(const std::string &name, double v)
{
	setAttribute(name, H5T_NATIVE_DOUBLE, &v);
}

void DataFile::setAttribute(const std::string &name, int v)
{
	setAttribute(name, H5T_NATIVE_INT, &v);
}

void DataFile::setAttribute(const std::string &name, const std::string &v)
{
	auto type = enforce(H5Tcopy(H5T_C_S1));
	enforce(H5Tset_size(type, H5T_VARIABLE));
	const char *ptr = v.c_str();
	setAttribute(name, type, &ptr);
	H5Tclose(type);
}

void DataFile::setAttribute(const std::string &name,
                            const std::vector<double> &v)
{
	setAttribute(name, H5T_NATIVE_DOUBLE, v.size(), v.data());
}

void DataFile::setAttribute(const std::string &name, const std::vector<int> &v)
{
	setAttribute(name, H5T_NATIVE_INT, v.size(), v.data());
}

void DataFile::getAttribute(const std::string &name, hid_t type, void *data)
{
	// open attribute
	auto attr = enforce(H5Aopen(id, name.c_str(), 0));

	// check size
	auto space = enforce(H5Aget_space(attr));
	auto size = H5Sget_simple_extent_npoints(space);
	H5Sclose(space);
	if (size != 1)
		throw std::runtime_error("HDF5 error: wrong attribute size");

	// read attribute
	enforce(H5Aread(attr, type, data));

	H5Aclose(attr);
}

template <> int DataFile::getAttribute<int>(const std::string &name)
{
	int r;
	getAttribute(name, H5T_NATIVE_INT, &r);
	return r;
}

template <> double DataFile::getAttribute<double>(const std::string &name)
{
	double r;
	getAttribute(name, H5T_NATIVE_DOUBLE, &r);
	return r;
}

template <>
std::string DataFile::getAttribute<std::string>(const std::string &name)
{
	auto type = enforce(H5Tcopy(H5T_C_S1));
	enforce(H5Tset_size(type, H5T_VARIABLE));

	char *ptr;
	getAttribute(name, type, &ptr);
	auto r = std::string(ptr);
	free(ptr);

	H5Tclose(type);
	return r;
}

template <>
std::vector<int>
DataFile::getAttribute<std::vector<int>>(const std::string &name)
{
	// open attribute
	auto attr = enforce(H5Aopen(id, name.c_str(), 0));

	// check size
	auto space = enforce(H5Aget_space(attr));
	auto size = H5Sget_simple_extent_npoints(space);
	auto r = std::vector<int>(size);
	H5Sclose(space);

	// read attribute
	enforce(H5Aread(attr, H5T_NATIVE_INT, r.data()));

	H5Aclose(attr);
	return r;
}

// explicit template instantitions
template void DataSet::write<float>(util::span<const float> data);
template void DataSet::write<double>(util::span<const double> data);
template void DataSet::write<int8_t>(util::span<const int8_t> data);
template void DataSet::write<int16_t>(util::span<const int16_t> data);
template void DataSet::write<int32_t>(util::span<const int32_t> data);

} // namespace util
