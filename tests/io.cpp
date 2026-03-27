#include "catch2/catch_test_macros.hpp"

#include "util/io.h"
#include <filesystem>
#include <string>

TEST_CASE("zstd text compression roundtrip", "[io]")
{
	const std::string text =
	    "This is a small text payload used for zstd compression tests.\n"
	    "It should survive a compress/decompress roundtrip exactly.";

	auto compressed = util::compress(text);
	auto decompressed = util::decompress(compressed);

	CHECK(decompressed == text);
}

TEST_CASE("zstd multi-frame decompression", "[io]")
{
	const std::string a = "first frame ";
	const std::string b = "second frame";

	auto ca = util::compress(a);
	auto cb = util::compress(b);
	ca.insert(ca.end(), cb.begin(), cb.end());

	auto decompressed = util::decompress(ca);
	CHECK(decompressed == a + b);
}

TEST_CASE("read_file auto-decompresses zstd frames", "[io]")
{
	const std::string text =
	    "Automatic zstd detection should trigger based on magic bytes.\n"
	    "The filename does not matter for this behavior.";
	const auto compressed = util::compress(text);

	const auto path = std::filesystem::temp_directory_path() /
	                  "util-zstd-read-file-auto-decompress.zst";
	struct RemoveFileOnExit
	{
		std::filesystem::path path;
		~RemoveFileOnExit() { std::filesystem::remove(path); }
	} cleanup{path};

	{
		auto file = util::File::create(path.string(), true);
		file.write_raw(compressed.data(), compressed.size());
	}
	const auto read_back = util::read_file(path.string());

	CHECK(read_back == text);
}
