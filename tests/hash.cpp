#include "catch2/catch_test_macros.hpp"

#include "util/hash.h"

using namespace util;

TEST_CASE("sha2/sha3 hash function test vectors", "[hash]")
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
