#include "catch2/catch_test_macros.hpp"

#include "util/io.h"
#include <chrono>
#include <filesystem>
#include <signal.h>
#include <string>
#include <thread>
#include <unistd.h>

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

// EventFd Tests

TEST_CASE("EventFd construction", "[io][eventfd]")
{
	util::EventFd efd(0);
	CHECK(efd.fd() >= 0);
	efd.close();
	CHECK(efd.fd() == -1);
}

TEST_CASE("EventFd write and read", "[io][eventfd]")
{
	util::EventFd efd(0);

	// Single write and read
	efd.write(1);
	uint64_t value = efd.read();
	CHECK(value == 1);

	// Multiple writes accumulate
	efd.write(5);
	efd.write(3);
	value = efd.read();
	CHECK(value == 8);

	efd.close();
}

TEST_CASE("EventFd try_read", "[io][eventfd]")
{
	util::EventFd efd(0);

	// try_read returns 0 when no data
	auto result = efd.try_read();
	CHECK(result == 0);

	// try_read returns value after write
	efd.write(42);
	result = efd.try_read();
	CHECK(result == 42);

	// try_read returns 0 after reading all data
	result = efd.try_read();
	CHECK(result == 0);

	efd.close();
}

TEST_CASE("EventFd write_safe", "[io][eventfd]")
{
	util::EventFd efd(0);

	// write_safe returns true on success
	bool success = efd.write_safe(1);
	CHECK(success);

	// Can still read normally
	uint64_t value = efd.read();
	CHECK(value == 1);

	efd.close();
}

TEST_CASE("EventFd blocking read with write from another thread",
          "[io][eventfd]")
{
	util::EventFd efd(0);

	std::atomic<bool> started{false};
	std::thread writer([&efd, &started]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		started = true;
		efd.write(123);
	});

	// This should block until writer writes
	uint64_t value = efd.read();
	CHECK(value == 123);
	CHECK(started);

	writer.join();
	efd.close();
}

// InterruptManager Tests

TEST_CASE("InterruptManager single instance constraint", "[io][interrupt]")
{
	{
		util::InterruptManager im1;

		// Creating a second instance should throw
		REQUIRE_THROWS_AS(util::InterruptManager{}, std::runtime_error);
	}
	// After destruction, should be able to create again
	{
		util::InterruptManager im3;
		// Destructor runs when im3 goes out of scope
	}
}

TEST_CASE("InterruptManager token acquisition", "[io][interrupt]")
{
	util::InterruptManager im;

	std::stop_token token = im.token();
	CHECK(!token.stop_requested());

	// Tokens should be copy-constructible and assignable
	std::stop_token token2 = token;
	CHECK(!token2.stop_requested());
}

TEST_CASE("InterruptManager token renewal on interrupt", "[io][interrupt]")
{
	util::InterruptManager im;

	std::stop_token token1 = im.token();
	CHECK(!token1.stop_requested());

	// Request stop via sending SIGINT
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	::kill(::getpid(), SIGINT);

	// Wait a bit for the signal to be processed
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// The first token should be stopped
	CHECK(token1.stop_requested());

	// But we can get a new one from InterruptManager
	std::stop_token token2 = im.token();
	CHECK(!token2.stop_requested());

	// Request stop again
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	::kill(::getpid(), SIGINT);

	// Wait for processing
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Now token2 should be stopped
	CHECK(token2.stop_requested());
}
