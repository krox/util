#ifdef UTIL_HDF5

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

Hdf5Dataset::Hdf5Dataset(hid_t id) : id_(id)
{
	if (id <= 0)
		return;
	auto space = enforce(H5Dget_space(id_));
	size_ = H5Sget_simple_extent_npoints(space);
	shape_.resize(64);
	auto rank = H5Sget_simple_extent_dims(space, &shape_[0], nullptr);
	shape_.resize(rank);
	H5Sclose(space);
}

void Hdf5Dataset::read_raw(hid_t type, void *data)
{
	assert(*this);
	enforce(H5Dread(id_, type, 0, 0, 0, data));
}

void Hdf5Dataset::read_raw(hsize_t row, hid_t type, void *data)
{
	assert(*this);
	assert(row < shape_[0]);

	auto offset = std::vector<hsize_t>(rank(), 0);
	offset[0] = row;
	auto memspace = enforce(H5Screate_simple(rank() - 1, &shape_[1], nullptr));
	std::vector<hsize_t> rowShape = shape_;
	rowShape[0] = 1;
	auto space = enforce(H5Dget_space(id_));
	H5Sselect_hyperslab(space, H5S_SELECT_SET, offset.data(), nullptr,
	                    rowShape.data(), nullptr);

	enforce(H5Dread(id_, type, memspace, space, 0, data));

	H5Sclose(memspace);
	H5Sclose(space);
}

void Hdf5Dataset::write_raw(hid_t type, void const *data)
{
	assert(*this);
	enforce(H5Dwrite(id_, type, 0, 0, 0, data));
}

void Hdf5Dataset::write_raw(hsize_t row, hid_t type, void const *data)
{
	assert(*this);
	assert(row < shape_[0]);

	auto offset = std::vector<hsize_t>(rank(), 0);
	offset[0] = row;
	auto memspace = enforce(H5Screate_simple(rank() - 1, &shape_[1], nullptr));
	std::vector<hsize_t> rowShape = shape_;
	rowShape[0] = 1;
	auto space = enforce(H5Dget_space(id_));
	H5Sselect_hyperslab(space, H5S_SELECT_SET, offset.data(), nullptr,
	                    rowShape.data(), nullptr);

	enforce(H5Dwrite(id_, type, memspace, space, 0, data));

	H5Sclose(memspace);
	H5Sclose(space);
}

Hdf5File Hdf5File::create(std::string const &filename, bool overwrite)
{
	auto mode = overwrite ? H5F_ACC_TRUNC : H5F_ACC_EXCL;
	auto id = enforce(H5Fcreate(filename.c_str(), mode, 0, 0));
	return Hdf5File(id);
}

Hdf5File Hdf5File::open(std::string const &filename, bool writeable)
{
	auto mode = writeable ? H5F_ACC_RDWR : H5F_ACC_RDONLY;
	auto id = enforce(H5Fopen(filename.c_str(), mode, 0));
	return Hdf5File(id);
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

Hdf5Dataset Hdf5File::create_data(std::string const &name,
                                  const std::vector<hsize_t> &size, hid_t type)
{
	assert(*this);
	auto space = enforce(H5Screate_simple(size.size(), size.data(), nullptr));
	auto props = enforce(H5Pcreate(H5P_DATASET_CREATE));

	if (size.size() > 0) // zero-dimensional datasets dont support chunks
	{
		auto chunk = guessChunkSize(size, type);
		H5Pset_chunk(props, chunk.size(), chunk.data());

		// enable compact encoding whenever the type has reduced precision
		auto typeSize = H5Tget_size(type);
		auto precision = H5Tget_precision(type); // 0 for compound types
		if (precision > 0 && precision < typeSize * 8)
			enforce(H5Pset_nbit(props));

		// checksum after (potentially lossy) compression
		enforce(H5Pset_fletcher32(props));
	}

	auto set = enforce(H5Dcreate2(id_, name.c_str(), type, space, 0, props, 0));
	H5Pclose(props);
	H5Sclose(space);
	return Hdf5Dataset(set);
}

Hdf5Dataset Hdf5File::open_data(std::string const &name)
{
	assert(*this);
	auto set = enforce(H5Dopen2(id_, name.c_str(), 0));
	return Hdf5Dataset(set);
}

bool Hdf5File::exists(std::string const &name)
{
	assert(*this);
	return enforce(H5Lexists(id_, name.c_str(), 0)) > 0;
}

void Hdf5File::remove(std::string const &name)
{
	assert(*this);
	enforce(H5Ldelete(id_, name.c_str(), 0));
}

void Hdf5File::make_group(std::string const &name)
{
	assert(*this);
	auto group = enforce(H5Gcreate2(id_, name.c_str(), 0, 0, 0));
	H5Gclose(group);
}

bool Hdf5File::has_attribute(const std ::string &name)
{
	// NOTE: H5Aexists returns negative/zero/positive on fail/no/yes
	assert(*this);
	return enforce(H5Aexists(id_, name.c_str()));
}

void Hdf5File::set_attribute_raw(std::string const &name, hid_t type,
                                 const void *v)
{
	assert(*this);
	auto space = enforce(H5Screate(H5S_SCALAR));
	auto attr = enforce(H5Acreate2(id_, name.c_str(), type, space, 0, 0));
	enforce(H5Awrite(attr, type, v));
	H5Aclose(attr);
	H5Sclose(space);
}

void Hdf5File::set_attribute_raw(std::string const &name, hid_t type,
                                 hsize_t count, const void *v)
{
	assert(*this);
	auto space = enforce(H5Screate_simple(1, &count, nullptr));
	auto attr = enforce(H5Acreate2(id_, name.c_str(), type, space, 0, 0));
	enforce(H5Awrite(attr, type, v));
	H5Aclose(attr);
	H5Sclose(space);
}

void Hdf5File::set_attribute(std::string const &name, double v)
{
	set_attribute_raw(name, H5T_NATIVE_DOUBLE, &v);
}

void Hdf5File::set_attribute(std::string const &name, int v)
{
	set_attribute_raw(name, H5T_NATIVE_INT, &v);
}

void Hdf5File::set_attribute(std::string const &name, std::string const &v)
{
	auto type = enforce(H5Tcopy(H5T_C_S1));
	enforce(H5Tset_size(type, H5T_VARIABLE));
	const char *ptr = v.c_str();
	set_attribute_raw(name, type, &ptr);
	H5Tclose(type);
}

void Hdf5File::set_attribute(std::string const &name,
                             const std::vector<double> &v)
{
	set_attribute_raw(name, H5T_NATIVE_DOUBLE, v.size(), v.data());
}

void Hdf5File::set_attribute(std::string const &name, const std::vector<int> &v)
{
	set_attribute_raw(name, H5T_NATIVE_INT, v.size(), v.data());
}

void Hdf5File::get_attribute_raw(std::string const &name, hid_t type,
                                 void *data)
{
	// open attribute
	auto attr = enforce(H5Aopen(id_, name.c_str(), 0));

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

template <> int Hdf5File::get_attribute<int>(std::string const &name)
{
	int r;
	get_attribute_raw(name, H5T_NATIVE_INT, &r);
	return r;
}

template <> double Hdf5File::get_attribute<double>(std::string const &name)
{
	double r;
	get_attribute_raw(name, H5T_NATIVE_DOUBLE, &r);
	return r;
}

template <>
std::string Hdf5File::get_attribute<std::string>(std::string const &name)
{
	auto type = enforce(H5Tcopy(H5T_C_S1));
	enforce(H5Tset_size(type, H5T_VARIABLE));

	char *ptr;
	get_attribute_raw(name, type, &ptr);
	auto r = std::string(ptr);
	free(ptr);

	H5Tclose(type);
	return r;
}

template <>
std::vector<int>
Hdf5File::get_attribute<std::vector<int>>(std::string const &name)
{
	// open attribute
	auto attr = enforce(H5Aopen(id_, name.c_str(), 0));

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

} // namespace util

#endif