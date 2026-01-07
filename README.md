# util

A general C++20 utility library (containers, numerics, I/O, concurrency, etc.).



## Use as library

This library is intended to be included via CMake, for example using the excellent [CPM](https://github.com/cpm-cmake/CPM.cmake) package manager, like this:

```cmake
CPMAddPackage(
	NAME util
	GITHUB_REPOSITORY krox/util
	GIT_TAG de3e57e3b98d1bd9954c1aae5d6cd2d8f7f97e21 # update to current head
	OPTIONS "UTIL_HDF5 ON" "UTIL_FFTW ON" # optional extra wrappers
)
```

## Tests

Using standard workflow
```sh
mkdir build && cd build
cmake ..
make

./tests # run unittests
```

## License

All code in this repository is released to the public domain. Feel free to do whatever you want with it. Attribution is appreciated but not required. Enjoy.

Of course, licenses for the upstream-libraries (FFTW, BLAKE3, libfmt, HDF5) still apply.

## Module index

- Containers and such: [vector](#vector), [hash_map](#hash_map), [bit_vector](#bit_vector-utilbit_vectorh), [span](#span-utilspanh), [ndarray](#ndarray), [ring_buffer](#ring_buffer), [memory](#memory-utilmemoryh)
- Numerical algorithms, data handling and visualization: [stats](#utilstatsh), [numerics](#utilnumericsh), [complex](#utilcomplexh), [ddouble](#utilddoubleh), [fft](#utilffth), [linalg](#linalg-utilinalgh), [random](#utilrandomh), [sampler](#utilsamplerh), [gnuplot](#utilgnuploth)
- File I/O: [json](#utiljsonh), [numpy](#utilnumpyh), [hdf5](#hdf5), [io](#utilioh)
- Multithreading support: [synchronized](#utilsynchronizedh), [threadpool](#utilthreadpoolh)
- Misc utilities: [hash](#utilhashh), [functional](#utilfunctionalh), [lazy](#utillazyh), [logging](#utilloggingh), [progressbar](#utilprogressbarh), [series](#utilseriesh), [simd](#utilsimdh)

# Containers and such

## vector

Multiple drop-in replacements for `std::vector` that are more efficient in certain cases (SBO, static, stable, ...).

```cpp
#include "util/vector.h"

void example()
{
    util::small_vector<int, 3> v; // stores up to 3 elements without allocation
    v.push_back(1);
    v.push_back(2);
    int last = v.pop_back();
}
```


## hash_map

Cache-friendly open-addressing hash table based on the "Swiss hashing" concepts. Should be a drop-in replacement for `std::unordered_map`, typically way more efficient.

```cpp
#include "util/hash_map.h"
#include <string>

void example() {
  util::hash_map<std::string, int> m;
  m["a"] = 1;
  if (auto it = m.find("a"); it != m.end())
    it->second += 1;
}
```

## bit_vector (`util/bit_vector.h`)

`bit_vector` is the same structure as a `std::vector<bool>`, but with a saner interface.

```cpp
#include "util/bit_vector.h"

void example1()
{
    // same structure as std::vector<bool>, but saner interface
    auto bits = util::bit_vector(1000);
    bits[15] = true;
    bits[17] = true;
    assert(bits.count() == 2);
}

void example2()
{
    // advanced version of bit_vector, with 'std::set<int>'-like API
    util::bit_set s;
    s.add(17);
    s.add(19);
    s.remove(17);
    assert(s.size() == 1);
}
```

## span (`util/span.h`)

Strided views (`gspan`), 2D views (`span_2d`), and N-D views (`ndspan`).

```cpp
#include "util/span.h"
#include <vector>

void example()
{
    std::vector<double> v = {1,2,3,4,5,6};
    util::gspan<double> xs(v.data(), 3, 2); // 1,3,5
    auto mid = xs.slice(1, 3);
    (void)mid;
}
```

## ndarray

Owning N-D array built on top of `ndspan` views.

```cpp
#include "util/ndarray.h"

void example() {
  util::ndarray<double, 2> a({3, 4});
  a(1, 2) = 5.0;
}
```

## ring_buffer

Fixed-capacity ring buffer that drops from the front when full.

```cpp
#include "util/ring_buffer.h"

void example()
{
  util::fixed_ring_buffer<int> rb(3);
  rb.push_back(1);
  rb.push_back(2);
  rb.push_back(3);
  rb.push_back(4); // drops 1
}
```

## memory (`util/memory.h`)

Allocation helpers and owning span types (`unique_span`, `unique_memory`, `lazy_memory`).

```cpp
#include "util/memory.h"

void example() {
  auto mem = util::allocate<int>(100);
  mem[0] = 123;
  // memory will be freed here
}
```


# Numerical algorithms, data handling and visualization

## `util/stats.h`

Basic statistics (mean/variance/cov/corr), fits, histograms, estimators.

```cpp
#include "util/stats.h"
#include <vector>

void example()
{
    std::vector<double> xs = {1,2,3,4};
    double m = util::mean(util::gspan<const double>(xs));
    util::Histogram h(xs, /*bins=*/5);
}
```

## `util/numerics.h`

1D root finding and numerical integration + accurate summation (`fsum`).

```cpp
#include "util/numerics.h"
#include <cmath>

void example()
{
  // solve the equation "cos(x)-x=0"
  double r = util::solve([](double x) { return std::cos(x) - x; }, 0.0, 1.0);

  // integrate a real function from 0 to 1
  double i = util::integrate([](double x) { return std::exp(-x*x); }, 0.0, 1.0);
}
```

## `util/complex.h`

A `std::complex`-like type optimized for performance and extensibility (e.g. `simd<T>`).

```cpp
#include "util/complex.h"

void example()
{
  util::complex<double> z(1.0, 2.0);
  auto a = util::abs(z);
  auto w = util::conj(z) * 3.0;
}
```

## `util/ddouble.h`

“Double-double” floating point type: ~107 bits of mantissa via sum of two doubles.

```cpp
#include "util/ddouble.h"

void example()
{
  util::ddouble x = util::ddouble::pi();
  util::ddouble y = util::ddouble::sum(1.0, 1e-16);
  util::ddouble z = x * y;
}
```

## `util/fft.h`

Wrapper around the excellent [FFTW](https://www.fftw.org/) library for fast Fourier transformations. Nicer, more C++-idiomatic interface than the library itself provides.
Available only when building with `-DUTIL_FFTW=ON`.

```cpp
#include "util/fft.h"
#include <vector>

void example() {
  int n = 1024;
  std::vector<util::complex<double>> in(n), out(n);
  util::fft_1d(in, out, FFTW_FORWARD);
}
```



## linalg (`util/linalg.h`)

Small fixed-size vectors/matrices (GLM-like) supporting mixed scalar types.

```cpp
#include "util/linalg.h"

void example()
{
  util::Vector<double, 3> a(1.0, 2.0, 3.0);
  util::Vector<double, 3> b(3.0, 2.0, 1.0);
  auto c = a + 2.0 * b;
}
```



## `util/random.h`

Fast PRNGs (e.g. `xoshiro256`) and some distributions that are missing from std `<random>` header.

```cpp
#include "util/random.h"

void example()
{
  util::xoshiro256 rng(123);
  auto x = util::truncated_normal_distribution(-1,3)(rng);
}
```

## `util/sampler.h`

Rejection sampler for log-concave-ish distributions via piecewise bounds.

```cpp
#include "util/sampler.h"
#include <random>

void example()
{
    // Sample from a (truncated) normal-like distribution on [-5, 5].
    auto f   = [](double x) { return -0.5 * x * x; };
    auto fd  = [](double x) { return -x; };
    auto fdd = [](double) { return -1.0; };

    util::LogSampler s(f, fd, fdd, -5.0, 5.0);
    util::xoshiro256 rng(123);
    double x = s(rng);
}
```

## `util/gnuplot.h`

Convenience wrapper around a gnuplot subprocess for quick plots.

```cpp
#include "util/gnuplot.h"
#include <vector>

void example()
{
  std::vector<double> ys = {1, 4, 9, 16};
  util::Gnuplot().plot_data(ys, "squares").range_x(0, 4);
}
```

## File IO


## `util/json.h`

A forgiving JSON/JSON5 DOM + serializer/deserializer.

```cpp
#include "util/json.h"

void example()
{
    // note that the string is not proper, conforming json
    util::Json j = util::Json::parse("{a: 1, b: [2, 3,],}");
    int a = j["a"].get<int>();
    j["c"] = 42;
}
```

## `util/numpy.h`

Read/write memory-mapped NumPy `.npy` arrays.

```cpp
#include "util/numpy.h"

void example()
{
  auto f = util::NumpyFile::create("a.npy", std::vector<size_t>{2, 3}, "<f8", true);
  auto a = f.view<double, 2>();
  a(0, 0) = 1.0;
}
```

## hdf5

Small C++ wrapper around the HDF5 C API.
Available only when building with `-DUTIL_HDF5=ON`.

```cpp
#include "util/hdf5.h"

#ifdef UTIL_HDF5
void example()
{
    auto f = util::Hdf5File::create("out.h5", /*overwrite=*/true);
    f.write_data("xs", std::vector<double>{1,2,3});
}
#endif
```

## `util/io.h`

RAII file handles, memory-mapped files, and convenience read/write helpers.

```cpp
#include "util/io.h"

void example()
{
  util::write_file("hello.txt", "hello\n");
  auto s = util::read_file("hello.txt");
  (void)s;
}
```

# Multithreading support

## `util/synchronized.h`

Thread-safe blocking queue (`synchronized_queue<T>`).

```cpp
#include "util/synchronized.h"

void example(
{
    util::synchronized_queue<int> q;
    q.push(1);
    auto v = q.try_pop();
}
```

## `util/threadpool.h`

A simple thread pool with cancellation semantics and convenience parallel loops.

```cpp
#include "util/threadpool.h"
#include <vector>

void example()
{
    util::ThreadPool pool(4);
    auto fut = pool.async([](int x) { return x * 2; }, 21);
    int y = fut.get();

    std::vector<int> xs = {1,2,3,4};
    pool.for_each(xs, [](int &v) { v *= 10; });
}
```

# Misc utilities

## `util/hash.h`

Hash functions and streaming hashers (SHA2-256, SHA3/SHAKE, BLAKE3, FNV-1a, Murmur3, …). Blake3 is just a binding to the (official, very optimized) library, the others are implemented here.

```cpp
#include "util/hash.h"
#include <fmt/format.h>

void example()
{
    auto sum = util::blake3("hello");
    fmt::print("{}\n", util::hex_string(sum));
}
```

## `util/functional.h`

`util::function_view`: a non-owning, type-erased callable reference for fast callbacks. Same as `std::function_ref` will be in C++26.

```cpp
#include "util/functional.h"

int call_twice(util::function_view<int(int)> f)
{
  return f(10) + f(10);
}
```

## `util/lazy.h`

Thread-safe “initialize on first use” wrapper (`util::synchronized_lazy`).

```cpp
#include "util/lazy.h"
#include <string>

void example() {
  util::synchronized_lazy init([] { return std::string("ready"); });
  auto &s = *init;
  (void)s;
}
```


## `util/logging.h`

fmt-based logger with simple per-component levels and a timing summary.

```cpp
#include "util/logging.h"

void example()
{
  util::Logger::set_level(util::Logger::Level::info);
  util::Logger log("main");
  log.info("hello {}", 42);
}
```

## `util/progressbar.h`

Simple terminal progress bar + RAII range wrapper.

```cpp
#include "util/progressbar.h"

void example()
{
  for (size_t i : util::ProgressRange(100)) {
    (void)i;
  }
}
```

## `util/series.h`

Fixed-order truncated power series with convenient arithmetic.

```cpp
#include "util/series.h"

void example()
{
    using S = util::Series<double, 4>;
    S x = S::generator();      // x
    S p = 1 + 2 * x + 3 * x*x; // 1 + 2x + 3x^2
}
```

## `util/simd.h`

A small SIMD wrapper with AVX/SSE backends (or OpenMP generic fallback).

```cpp
#include "util/simd.h"

void example()
{
    util::simd<float> a(1.0f);
    util::simd<float> b = util::sqrt(a * a + 1.0f);
    float s = util::vsum(b);
}
```


## `util/stopwatch.h`

Timing utilities: `Stopwatch` and RAII `StopwatchGuard`.

```cpp
#include "util/stopwatch.h"

void example()
{
    util::Stopwatch sw;
    {
        util::StopwatchGuard g(sw);
        // work...
    }
    double secs = sw.secs();
}
```

## `util/string.h`

String utilities (trim/split) + a lightweight pull-parser (`util::Parser`).

```cpp
#include "util/string.h"

void example()
{
    util::Parser p("foo=123", {.comment_start = "#"});
    auto key = p.expect_ident();
    p.expect('=');
    int value = p.expect_int<int>();
}
```

## `util/string_id.h`

String interning via `StringPool` and compact `string_id` handles.

```cpp
#include "util/string_id.h"

void example()
{
    util::StringPool pool;
    util::string_id a = pool.id("hello");
    util::string_id b = pool("hello");
    auto s = pool(a);
}
```



