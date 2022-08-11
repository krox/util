#include "catch2/catch_test_macros.hpp"

#include "util/hash.h"
#include <span>

using namespace util;

std::span<const std::byte> as_bytes(std::string_view s)
{
	return std::as_bytes(std::span(s.data(), s.size()));
}

TEST_CASE("sha2/sha3 hash function test vectors", "[hash][sha]")
{
	auto msg = as_bytes("");
	CHECK(hex_string(sha256(msg)) ==
	      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	CHECK(hex_string(sha3<224>(msg)) ==
	      "6b4e03423667dbb73b6e15454f0eb1abd4597f9a1b078e3f5b5a6bc7");
	CHECK(hex_string(sha3<256>(msg)) ==
	      "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a");
	CHECK(hex_string(sha3<384>(msg)) ==
	      "0c63a75b845e4f7d01107d852e4c2485c51a50aaaa94fc61995e71bbee983a2a"
	      "c3713831264adb47fb6bd1e058d5f004");
	CHECK(hex_string(sha3<512>(msg)) ==
	      "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a6"
	      "15b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26");

	msg = as_bytes("foobar");
	CHECK(hex_string(sha256(msg)) ==
	      "c3ab8ff13720e8ad9047dd39466b3c8974e592c2fa383d4a3960714caef0c4f2");
	CHECK(hex_string(sha3<224>(msg)) ==
	      "1ad852ba147a715fe5a3df39a741fad08186c303c7d21cefb7be763b");
	CHECK(hex_string(sha3<256>(msg)) ==
	      "09234807e4af85f17c66b48ee3bca89dffd1f1233659f9f940a2b17b0b8c6bc5");
	CHECK(hex_string(sha3<384>(msg)) ==
	      "0fa8abfbdaf924ad307b74dd2ed183b9a4a398891a2f6bac8fd2db7041b77f06"
	      "8580f9c6c66f699b496c2da1cbcc7ed8");
	CHECK(hex_string(sha3<512>(msg)) ==
	      "ff32a30c3af5012ea395827a3e99a13073c3a8d8410a708568ff7e6eb85968fc"
	      "cfebaea039bc21411e9d43fdb9a851b529b9960ffea8679199781b8f45ca85e2");

	// sha3-512 takes 72 bytes per block, at least one byte padding.
	// so we test 71,72,73 byte messages
	msg = as_bytes("165bff95bcff75fd65dbaa5f17990cdfd2bbbb2ef438898b0e49d78e915"
	               "e67abbc0cf7c");
	CHECK(hex_string(sha3<512>(msg)) ==
	      "f4203c447f9917addc2ffd87724a5360b73c900c13527f46bf51ba12d37e8107"
	      "d55efdf4bf9e936fac392a8192c6f4889eb4cfc95114c6ad11635fd59688944b");
	msg = as_bytes("165bff95bcff75fd65dbaa5f17990cdfd2bbbb2ef438898b0e49d78e915"
	               "e67abbc0cf7ca");
	CHECK(hex_string(sha3<512>(msg)) ==
	      "a5aaabcb0bec76ca439ef2b0a44e034b3dba55231fa4626ebb02bd2cc4be0996"
	      "07c07a757ee06deb4940f2b9e9c124bc2f975781e3b7540453f82360595a71f5");
	msg = as_bytes("165bff95bcff75fd65dbaa5f17990cdfd2bbbb2ef438898b0e49d78e915"
	               "e67abbc0cf7ca4");
	CHECK(hex_string(sha3<512>(msg)) ==
	      "abb54ee3af0f51d44fc8f066028a1571ea23c2f348398d56defc5d2c2006e5cb"
	      "7c38eab0837ddd274f42181da5971427a05e2029f2ad28adf0cff1d3d7f53479");
}

TEST_CASE("blake3 test vectors", "[hash][blake3]")
{
	// we rely on the official blake3 implementation, so tests here are brief
	CHECK(hex_string(blake3("")) ==
	      "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262");
	CHECK(hex_string(blake3("foobar")) ==
	      "aa51dcd43d5c6c5203ee16906fd6b35db298b9b2e1de3fce81811d4806b76b7d");

	// check incremental interface
	// (blake3 library itself offers an interface with abitrary seeking)
	std::array<std::byte, 16> half;
	Blake3 blake3;
	blake3("foo", 3);
	blake3("bar", 3);
	blake3.generate_bytes(&half, sizeof(half));
	CHECK(hex_string(half) == "aa51dcd43d5c6c5203ee16906fd6b35d");
	blake3.generate_bytes(&half, sizeof(half));
	CHECK(hex_string(half) == "b298b9b2e1de3fce81811d4806b76b7d");
}

TEST_CASE("murmur3 test vectors", "[hash][murmur]")
{
	// data is processed in 16-byte blocks, so tests up to 17 bytes seem
	// reasonable. Tested against https://asecuritysite.com/hash/mur
	CHECK(hex_string(murmur3_128("")) == "00000000000000000000000000000000");
	CHECK(hex_string(murmur3_128("f")) == "afa3664e2d13439221e8d041382a4dc1");
	CHECK(hex_string(murmur3_128("fo")) == "f26f7ee42441a01803ce13963177a269");
	CHECK(hex_string(murmur3_128("foo")) == "6145f501578671e2877dba2be487af7e");
	CHECK(hex_string(murmur3_128("foob")) ==
	      "f8ea585d207f74d2fabe264b60dbbdfa");
	CHECK(hex_string(murmur3_128("fooba")) ==
	      "19f951bfdd2f21f26642dda789509842");
	CHECK(hex_string(murmur3_128("foobar")) ==
	      "455ac81671aed2bdafd6f8bae055a274");
	CHECK(hex_string(murmur3_128("foobar1")) ==
	      "acfbfffbb8ce0ed0e50b31f794cb76d1");
	CHECK(hex_string(murmur3_128("foobar12")) ==
	      "98707f421e62fdf0d5e8c9e7dfc5d65d");
	CHECK(hex_string(murmur3_128("foobar123")) ==
	      "6953c4b62e251b6c24b91c657bffe0ac");
	CHECK(hex_string(murmur3_128("foobar1234")) ==
	      "e701463ab5401598133ca33065627f7e");
	CHECK(hex_string(murmur3_128("foobar12345")) ==
	      "e6f35c3cf32a97a50f173814482a959c");
	CHECK(hex_string(murmur3_128("foobar123456")) ==
	      "61095035d45820dd452ff1d7eccbbb5b");
	CHECK(hex_string(murmur3_128("foobar1234567")) ==
	      "9ec2350eca8190cf106d1b86a2d3ae22");
	CHECK(hex_string(murmur3_128("foobar12345678")) ==
	      "c49a31f2ed6ab5bc6bcd5efba65819fc");
	CHECK(hex_string(murmur3_128("foobar123456789")) ==
	      "f66c91af62d680b90dc4992bf9e7e99c");
	CHECK(hex_string(murmur3_128("foobar1234567890")) ==
	      "87765c1243d0e61a88304e6b6f6ef810");
	CHECK(hex_string(murmur3_128("foobar1234567890x")) ==
	      "190510f5490855d9c904ad00a7381c41");

	// "official" test vector from https://github.com/aappleby/smhasher/issues/6
	CHECK(hex_string(
	          murmur3_128("The quick brown fox jumps over the lazy dog")) ==
	      "6c1b07bc7bbc4be347939ac4a93c437a");

	// "incremental" interface
	{
		Murmur3 m;
		m(nullptr, 0);
		m("foo", 3);
		m("bar", 3);
		CHECK(((std::array<std::byte, 16>)m == murmur3_128("foobar")));
	}

	{
		Murmur3 m;
		m("The quick brown f", 17);
		m("ox jumps over the", 17);
		m(" lazy dog", 9);
		CHECK((static_cast<std::array<std::byte, 16>>(m) ==
		       murmur3_128("The quick brown fox jumps over the lazy dog")));
	}
}

TEST_CASE("util::hash", "[hash]")
{
	CHECK(is_contiguously_hashable_v<int> == true);
	CHECK(is_contiguously_hashable_v<int *> == true);
	CHECK(is_contiguously_hashable_v<std::vector<int>> == false);
	CHECK(is_contiguously_hashable_v<std::pair<int, int>> == true);
	CHECK(is_contiguously_hashable_v<std::pair<char, int>> == false);

	CHECK(hash<int>()(5) != 5);
	CHECK(hash<std::pair<char, int>>()({1, 2}) !=
	      hash<std::pair<int, int>>()({1, 2}));

	CHECK(seeded_hash<int>(1)(5) != seeded_hash<int>(2)(5));
}
