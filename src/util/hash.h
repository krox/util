#ifndef UTIL_HASH_H
#define UTIL_HASH_H

#include "util/span.h"
#include <array>
#include <cstddef>

namespace util {

/** SHA3-256, aka a variant of Keccak */
std::array<std::byte, 32> sha3_256(span<const std::byte> data);

/** convenience function for pretty printing hash sums */
std::string hex_string(span<const std::byte> h);

} // namespace util

#endif
