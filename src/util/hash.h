#pragma once

#include "util/span.h"
#include <array>
#include <cstddef>

namespace util {

/** SHA3-256, aka a variant of Keccak */
std::array<std::byte, 32> sha3_256(span<const std::byte>);

/** SHA2-256 */
std::array<std::byte, 32> sha256(span<const std::byte>);

/** FNV-1a */
uint32_t fnv1a_32(span<const std::byte>);
uint64_t fnv1a_64(span<const std::byte>);

/** convenience function for pretty printing hash sums */
std::string hex_string(span<const std::byte>);

} // namespace util
