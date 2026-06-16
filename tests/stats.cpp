#include "catch2/catch_test_macros.hpp"

#include "util/stats.h"

using namespace util;

TEST_CASE("IntHistogram2D basic operations")
{
	IntHistogram2D hist;

	// Add some values
	hist.add(0, 0);
	hist.add(1, 1);
	hist.add(2, 2);

	// Check that values were added correctly
	CHECK(hist(0, 0) == 1);
	CHECK(hist(1, 1) == 1);
	CHECK(hist(2, 2) == 1);

	// Check max values
	CHECK(hist.max_x() == 2);
	CHECK(hist.max_y() == 2);

	// Check out-of-bounds returns 0
	CHECK(hist(3, 3) == 0);
	CHECK(hist(-1, -1) == 0);
}

TEST_CASE("IntHistogram2D weighted operations")
{
	IntHistogram2D hist;

	// Add weighted values
	hist.add(0, 0, 5);
	hist.add(1, 1, 10);
	hist.add(0, 0, 3); // Add more weight to (0, 0)

	CHECK(hist(0, 0) == 8);
	CHECK(hist(1, 1) == 10);
}

TEST_CASE("IntHistogram2D span constructor")
{
	std::vector<std::pair<int, int>> points = {{0, 0}, {1, 1}, {2, 2}, {1, 1}};

	IntHistogram2D hist = IntHistogram2D(std::span(points));

	CHECK(hist(0, 0) == 1);
	CHECK(hist(1, 1) == 2);
	CHECK(hist(2, 2) == 1);
	CHECK(hist.max_x() == 2);
	CHECK(hist.max_y() == 2);
}

TEST_CASE("IntHistogram2D add span")
{
	IntHistogram2D hist;

	std::vector<std::pair<int, int>> points = {{0, 0}, {1, 0}, {0, 1}};

	hist.add(std::span(points));

	CHECK(hist(0, 0) == 1);
	CHECK(hist(1, 0) == 1);
	CHECK(hist(0, 1) == 1);
}

TEST_CASE("IntHistogram2D default constructor")
{
	IntHistogram2D hist;

	// Empty histogram should return 0 for any query
	CHECK(hist(0, 0) == 0);
	CHECK(hist(100, 100) == 0);
}

TEST_CASE("IntHistogram2D resizing")
{
	IntHistogram2D hist;

	// Add values that will trigger resizing
	hist.add(5, 5);
	hist.add(10, 10);
	hist.add(20, 20);

	CHECK(hist(5, 5) == 1);
	CHECK(hist(10, 10) == 1);
	CHECK(hist(20, 20) == 1);
	CHECK(hist.max_x() == 20);
	CHECK(hist.max_y() == 20);
}

TEST_CASE("DoubleHistogram basic operations")
{
	DoubleHistogram hist;

	// Add some values
	hist.add(0.0);
	hist.add(1.0);
	hist.add(2.0);

	CHECK(hist.count() == 3);
	CHECK(hist.num_bins() > 0);
	CHECK(hist.bin_size() >= 1.0); // bin size is power of two
}

TEST_CASE("DoubleHistogram weighted values")
{
	DoubleHistogram hist;

	hist.add(0.5, 5);
	hist.add(0.7, 3);
	hist.add(1.5, 2);

	CHECK(hist.count() == 10);
	CHECK(hist.num_bins() > 0);
}

TEST_CASE("DoubleHistogram negative values")
{
	DoubleHistogram hist;

	hist.add(-5.0);
	hist.add(-2.5);
	hist.add(0.0);
	hist.add(2.5);

	CHECK(hist.count() == 4);
	CHECK(hist.num_bins() > 0);
}

TEST_CASE("DoubleHistogram coarsening")
{
	DoubleHistogram hist;

	// Add many values to potentially trigger coarsening
	// With step 0.1, we'd need 10000 values to reach 1000 bins
	for (int i = 0; i < 5000; ++i)
		hist.add(i * 0.01); // step of 0.01

	CHECK(hist.count() == 5000);
	// After coarsening, num_bins should be <= MAX_BINS (1000)
	CHECK(hist.num_bins() <= 1000);
}

TEST_CASE("DoubleHistogram span constructor")
{
	std::vector<double> values = {0.1, 0.5, 1.2, 1.8, 2.3};
	DoubleHistogram hist = DoubleHistogram(std::span(values));

	CHECK(hist.count() == 5);
	CHECK(hist.num_bins() > 0);
}

TEST_CASE("DoubleHistogram extreme values")
{
	DoubleHistogram hist;

	// Add small value first
	hist.add(1.0);

	// Then add extremely large value (should trigger coarsening)
	hist.add(1e10, 5);

	CHECK(hist.count() == 6);
	CHECK(hist.num_bins() <= 1000); // Should have coarsened

	// Add extremely small (negative) value
	hist.add(-1e10, 3);

	CHECK(hist.count() == 9);
	CHECK(hist.num_bins() <= 1000); // Should still be under limit
}
