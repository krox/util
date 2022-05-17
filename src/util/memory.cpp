#include "util/memory.h"

#include <cassert>
#include <sys/mman.h>

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
} // namespace util
