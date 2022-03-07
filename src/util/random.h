#pragma once

/**
 * Pseudorandom number generators and distributions.
 *
 * Similar overall design to <random> header, but not fully compatible
 *
 *     - using a generator from here with a std distribution works, the other
 *       way around does not
 *     - can call '.uniform()', '.normal()', and '.bernoulli()' directly on the
 *       generator, without explicitly creating a distribution object
 *     - distributions provide additional information about themselves, such as
 *       '.mean()', '.variance()', ...
 *     - slight biases in the distributions are acceptable for the sake of
 *       performance as long as no simulation of practical scale can detect it
 *     - distributions are not templated, real numbers are fixed as 'double'
 *           * just being able to switch between 'float' and 'double' does
 *             not seem overly useful (this would change if simd<float/double>
 *             were supported)
 *           * in more complicated situations, it is unclear (to me) whether
 *             we want to template the parameter type or the result type
 *
 * TODO: should be able to produce multiple independent samples using SIMD
 */

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <random>
#include <stdint.h>
#include <tuple>

namespace util {

/**
 * Originally written in 2015 by Sebastiano Vigna (vigna@acm.org).
 * public domain, taken from http://xoroshiro.di.unimi.it/splitmix64.c
 */
class splitmix64
{
	uint64_t s = 0; // all values are allowed

  public:
	splitmix64() = default;
	explicit splitmix64(uint64_t x) : s(x) {}

	using result_type = uint64_t;
	uint64_t min() const { return 0; }
	uint64_t max() const { return UINT64_MAX; }

	void seed(uint64_t x) { s = x; }

	uint64_t operator()()
	{
		s += 0x9e3779b97f4a7c15;
		uint64_t z = s;
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
		z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
		return z ^ (z >> 31);
	}
};

/**
 * This is xoshiro256**, version 1.0.
 * Originally written in 2018 by David Blackman and Sebastiano Vigna.
 * public domain, from http://xoshiro.di.unimi.it/xoshiro256starstar.c
 */
class xoshiro256
{
	uint64_t s[4]; // should not be all zeroes

	static inline uint64_t rotl(uint64_t x, int k)
	{
		// NOTE: compiler will optimize this to a single instruction
		return (x << k) | (x >> (64 - k));
	}

  public:
	xoshiro256() { seed(0); }
	explicit xoshiro256(uint64_t x) { seed(x); }
	explicit xoshiro256(uint64_t x, uint64_t y) { seed(x, y); }
	explicit xoshiro256(std::array<std::byte, 32> const &v) { seed(v); }

	using result_type = uint64_t;
	uint64_t min() const { return 0; }
	uint64_t max() const { return UINT64_MAX; }

	/** set the internal state using a 64 bit seed */
	void seed(uint64_t x)
	{
		splitmix64 gen(x);
		s[0] = gen();
		s[1] = gen();
		s[2] = gen();
		s[3] = gen();
	}

	/** set the internal state using a 128 bit seed */
	void seed(uint64_t x, uint64_t y)
	{
		splitmix64 gen1(x);
		splitmix64 gen2(y);
		s[0] = gen1();
		s[1] = gen1();
		s[2] = gen2();
		s[3] = gen2();
	}

	// set internal state directly
	//     * use with care, there are some bad regions (e.g. all/most bits zero)
	//     * intended to be used as something like
	//           seed(sha3<256>("human_readable_seed_of_arbitrary_length"))
	void seed(std::array<std::byte, 32> const &v)
	{
		std::memcpy(s, v.data(), 32);
	}

	void advance()
	{
		uint64_t t = s[1] << 17;
		s[2] ^= s[0];
		s[3] ^= s[1];
		s[1] ^= s[2];
		s[0] ^= s[3];
		s[2] ^= t;
		s[3] = rotl(s[3], 45);
	}

	// this is the '**' output function
	uint64_t generate()
	{
		uint64_t result = rotl(s[1] * 5, 7) * 9;
		advance();
		return result;
	}

	// this is the '++' output function. Slightly faster than '**', but with
	// a slight statistical weakness in the lowest few bits, which
	uint64_t generate_fast()
	{
		const uint64_t result = rotl(s[0] + s[3], 23) + s[0];
		advance();
		return result;
	}

	// generate next value in the random sequence
	uint64_t operator()() { return generate(); }

	/**
	 * generate uniform value in [0,1].
	 * Essentially equivalent to
	 *     std::uniform_real_distribution<double>(0,1)(*this)
	 * But faster.
	 */
	template <typename T = double> T uniform()
	{
		// NOTE: the statistical weakness of generate_fast() is mostly in the
		//       low bits, which are typically not used when converting to a
		//       floating point number, so this is okay here.

		// this version can return 1.0 (depending on rounding mode)
		return generate_fast() * 0x1p-64;

		// this version is strictly in [0,1) (independent of rounding mode)
		// return (generate_fast() >> 11) * 0x1p-53;
	}

	// generate a value with normal/Gaussian distribution (µ=0, σ²=1)
	template <typename T = double> T normal()
	{
		// tables for the Ziggurat method
		auto pdf = [](double x) { return std::exp(-0.5 * x * x); };
		constexpr int n = 16; // must be power of two
		constexpr double table_x[17] = {
		    0,
		    0.5760613949656382,
		    0.7848844962025341,
		    0.9423784527652854,
		    1.0773743224753307,
		    1.200704026435259,
		    1.3180610326087927,
		    1.4332000178637592,
		    1.5491474170121649,
		    1.6688615282467072,
		    1.7958043759924367,
		    1.9347422398932554,
		    2.093335394648163,
		    2.2862554378205204,
		    2.5498700041250193,
		    3.0419762337330707,
		    9,
		};
		constexpr double table_y[17] = {
		    1,
		    0.8471111497389042,
		    0.734899270434089,
		    0.641440677341622,
		    0.5596925211819822,
		    0.4863410853434781,
		    0.41952068615317745,
		    0.35806843715908643,
		    0.3012156396855146,
		    0.24844112073029095,
		    0.1993971571819638,
		    0.15387514265202898,
		    0.11180192085428531,
		    0.0732789444190452,
		    0.03873860933779797,
		    0.00978592937289994,
		    2.576757109154981e-18,
		};

		// implementaion notes:
		//    * a uniform double does not (usually) use the low bits of the
		//      random 64-bit value. Therefore we can simply use some for
		//      randomly selecting the layer and the sign
		//    * only ~2^-64 of the pdf is outside of a 9-sigma radius. Therefore
		//      it will not be practically noticable if we just cur off there
		//    * implementation could be slightly optimized by pre-multiplying
		//      tablex by 2^-64 and such ideas
		//   * also, for maximal performance, one might use exponential tails
		//     instead of truncation, and also bigger tables

		auto u = (*this)();
		auto i = (size_t)(u & (n - 1));
		auto x = u * 0x1.0p-64;
		x *= table_x[i + 1];
		if (x > table_x[i])
		{
			if (table_y[i + 1] + uniform() * (table_y[i] - table_y[i + 1]) >
			    pdf(x))
				return normal();
		}
		return (u & n) ? x : -x;
	}

	// bernoulli with p=1/2
	bool bernoulli() { return generate_fast() & (1UL << 63); }

	/** start a new generator, seeded by values from this one */
	xoshiro256 split()
	{
		// This splitting method was not really well studied/tested for
		// statistical robustness. But using a 128 bit seed with some scrambling
		// provided by the splitmix64 inside of the constructor is hopefully
		// good enough to avoid any problems in practice.
		return xoshiro256((*this)(), (*this)());
	}

	/** discards 2^128 values of the random sequence */
	void jump()
	{
		static const uint64_t JUMP[] = {0x180ec6d33cfd0aba, 0xd5a61266f0c9392c,
		                                0xa9582618e03fc9aa, 0x39abdc4529b1661c};

		uint64_t s0 = 0;
		uint64_t s1 = 0;
		uint64_t s2 = 0;
		uint64_t s3 = 0;
		for (int i = 0; i < 4; i++)
			for (int b = 0; b < 64; b++)
			{
				if (JUMP[i] & UINT64_C(1) << b)
				{
					s0 ^= s[0];
					s1 ^= s[1];
					s2 ^= s[2];
					s3 ^= s[3];
				}
				(*this)();
			}

		s[0] = s0;
		s[1] = s1;
		s[2] = s2;
		s[3] = s3;
	}
};

class uniform_distribution
{
	double a_ = 0.0, b_ = 1.0, w_ = 1.0;

  public:
	using result_type = double;

	// constructors
	uniform_distribution() = default;
	uniform_distribution(double a, double b) : a_(a), b_(b), w_(b - a)
	{
		assert(a <= b);
	}

	// parameters
	double a() const { return a_; }
	double b() const { return b_; }

	// properties
	double min() const { return a_; }
	double max() const { return b_; }
	double mean() const { return 0.5 * (a_ + b_); }
	double variance() const { return (1. / 12.) * (b_ - a_) * (b_ - a_); }
	double skewness() const { return 0.; }
	double kurtosis() const { return -6. / 5.; }

	// generator
	template <class Rng> double operator()(Rng &rng)
	{
		return a_ + rng.uniform() * w_;
	}
};

class bernoulli_distribution
{
	double p_ = 0.5; // values outside [0,1] are allowed and clamped implicitly

  public:
	using result_type = bool;

	// constructors
	bernoulli_distribution() = default;
	explicit bernoulli_distribution(double p) : p_(p) {}

	// parameters
	double p() const { return std::clamp(p_, 0.0, 1.0); }
	double q() const { return 1 - p(); }

	// properties
	double min() const { return 0; }
	double max() const { return 1; }
	double mean() const { return p(); }
	double variance() const { return p() * q(); }
	double skewness() const { return (q() - p()) / std::sqrt(p() * q()); }
	double kurtosis() const { return (1 - 6 * p() * q()) / (p() * q()); }

	// generator
	template <class Rng> result_type operator()(Rng &rng)
	{
		return rng.uniform() <= p_;
	}
};

class normal_distribution
{
	double mu_ = 0.0, sigma_ = 1.0;

  public:
	using result_type = double;

	// constuctors
	normal_distribution() = default;
	normal_distribution(double mu, double sigma) : mu_(mu), sigma_(sigma)
	{
		assert(sigma > 0);
	}

	// parameters
	double mu() const { return mu_; }
	double sigma() const { return sigma_; }

	// properties
	double min() const { return -std::numeric_limits<double>::infinity(); }
	double max() const { return std::numeric_limits<double>::infinity(); }
	double mean() const { return mu_; }
	double variance() const { return sigma_ * sigma_; }
	double skewness() const { return 0.0; }
	double kurtosis() const { return 0.0; }

	// generator
	template <class Rng> result_type operator()(Rng &rng)
	{
		return rng.normal() * sigma_ + mu_;
	}
};

class exponential_distribution
{
	double lambda_ = 1.0;

  public:
	using result_type = double;

	// constructors
	exponential_distribution() = default;
	explicit exponential_distribution(double lambda) : lambda_(lambda)
	{
		assert(lambda > 0);
	}

	// parameters
	double lambda() const { return lambda_; }

	// properties
	double min() const { return 0.0; }
	double max() const { return std::numeric_limits<double>::infinity(); }
	double mean() const { return 1.0 / lambda_; }
	double variance() const { return 1.0 / (lambda_ * lambda_); }
	double skewness() const { return 2.0; }
	double kurtosis() const { return 6.0; }

	// generator
	template <class Rng> result_type operator()(Rng &rng)
	{
		return -std::log(rng.uniform()) / lambda_;
	}
};

class binomial_distribution
{
	int n_ = 1;
	double p_ = 0.5;

  public:
	using result_type = int;

	// constructors
	binomial_distribution() = default;
	binomial_distribution(int n, double p) : n_(n), p_(p)
	{
		assert(n >= 0);
		assert(0 <= p && p <= 1.0);
	}

	// parameters
	int n() const { return n_; }
	double p() const { return p_; }
	double q() const { return 1.0 - p_; }

	// properties
	double min() const { return 0; }
	double max() const { return n(); }
	double mean() const { return n() * p(); }
	double variance() const { return n() * p() * q(); }
	double skewness() const { return (q() - p()) / std::sqrt(variance()); }
	double kurtosis() const { return (1.0 - 6 * p() * q()) / variance(); }

	// generator
	template <class Rng> result_type operator()(Rng &rng)
	{
		// TODO: this is not really a reasonable algorithm...
		int count = 0;
		for (int i = 0; i < n_; ++i)
			if (rng.uniform() <= p_)
				++count;
		return count;
	}
};

class poisson_distribution
{
	double lambda_ = 1.0;

  public:
	using result_type = int;

	// constructors
	poisson_distribution() = default;
	explicit poisson_distribution(double lambda) : lambda_(lambda)
	{
		assert(lambda > 0);
	}

	// parameters
	double lambda() const { return lambda_; }

	// properties
	double min() const { return 0; }
	double max() const { return std::numeric_limits<double>::infinity(); }
	double mean() const { return lambda(); }
	double variance() const { return lambda(); }
	double skewness() const { return 1.0 / std::sqrt(lambda()); }
	double kurtosis() const { return 1.0 / lambda(); }

	// generator
	template <class Rng> result_type operator()(Rng &rng)
	{
		double L = exp(-lambda_);
		double p = rng.uniform();
		int k = 0;
		while (p > L)
		{
			p *= rng.uniform();
			k += 1;
		}
		return k;
	}
};

class truncated_normal_distribution
{
	// TODO: performance can break down if the two limits are close together or
	//       if sampling far into the tail. Fallback could be a simple
	//       exponential proposal

	// parameters of the distribution
	double mean_, stddev_;
	double low_, high_; // normalized to mean/stddev

	std::uniform_real_distribution<double> uniform;
	std::exponential_distribution<double> exponential;

	// performance statistics
	int64_t nSamples = 0, nTries = 0;

	// (x,f(x)) pairs of f(x)=e^(-x^2/2), such that the 2*N+2 upper
	// approximations (2N x rectangles + 2 x exponential) have the same
	// area. should yield ~96% acceptance (~90% without evaluating f)
	static constexpr int N = 32;
	static constexpr double table_x[2 * N + 1] = {
	    -2.2088991613469996798555088, -1.9464639554256921438020565,
	    -1.7605321487820659728268064, -1.6150755480872587551731726,
	    -1.4944865272660961059472554, -1.3906700366261811710537086,
	    -1.2989059026332570543845453, -1.2162057486771537967471211,
	    -1.1405585677177464252394445, -1.0705458139604808487101670,
	    -1.0051286264475409945049639, -0.9435225416179405324508475,
	    -0.8851198584184690287180222, -0.8294394605593851786920640,
	    -0.7760932155237990425671081, -0.7247627832459330710236253,
	    -0.6751831852245773913177646, -0.6271308934056000741736992,
	    -0.5804150181228240335064692, -0.5348706685049481067980362,
	    -0.4903538657056535981591895, -0.4467375851846964155589596,
	    -0.4039086322763870675860141, -0.3617651407382518330540025,
	    -0.3202145421114487829450796, -0.2791718939440860883320001,
	    -0.2385584831563439744813612, -0.1983006408748560979282588,
	    -0.1583287194281477011813734, -0.1185761925313981673632577,
	    -0.0789788471085439537793217, -0.0394740404513923029424662,
	    0.0000000000000000000000000,  0.0394740404513923029424662,
	    0.0789788471085439537793217,  0.1185761925313981673632577,
	    0.1583287194281477011813734,  0.1983006408748560979282588,
	    0.2385584831563439744813612,  0.2791718939440860883320001,
	    0.3202145421114487829450796,  0.3617651407382518330540025,
	    0.4039086322763870675860141,  0.4467375851846964155589596,
	    0.4903538657056535981591895,  0.5348706685049481067980362,
	    0.5804150181228240335064692,  0.6271308934056000741736992,
	    0.6751831852245773913177646,  0.7247627832459330710236253,
	    0.7760932155237990425671081,  0.8294394605593851786920640,
	    0.8851198584184690287180222,  0.9435225416179405324508475,
	    1.0051286264475409945049639,  1.0705458139604808487101670,
	    1.1405585677177464252394445,  1.2162057486771537967471211,
	    1.2989059026332570543845453,  1.3906700366261811710537086,
	    1.4944865272660961059472554,  1.6150755480872587551731726,
	    1.7605321487820659728268064,  1.9464639554256921438020565,
	    2.2088991613469996798555088};
	static constexpr double table_low[2 * N] = {
	    0.0871941748480579986507079, 0.1504144244397940443049223,
	    0.2123038610981274593794731, 0.2713801935617592170277299,
	    0.3273435689467415081331061, 0.3802290003069655597718239,
	    0.4301685062972002231898738, 0.4773151991028322058895413,
	    0.5218177326736637589389453, 0.5638121389746919174509679,
	    0.6034200177680236694377441, 0.6407490519901669671243287,
	    0.6758942960988735968719870, 0.7089396263168475492411578,
	    0.7399591184920328886298624, 0.7690182743388618472684144,
	    0.7961750806125826782846465, 0.8214809108397780763011192,
	    0.8449812876768730110546447, 0.8667165253776934352758185,
	    0.8867222704506056271411144, 0.9050299562436422081151588,
	    0.9216671847173235372141197, 0.9366580463717067359685986,
	    0.9500233872907135082348609, 0.9617810305618204273733857,
	    0.9719459578929604660943619, 0.9805304560384748635197798,
	    0.9875442316181902420869293, 0.9929944970270556052085794,
	    0.9968860293500699344572167, 0.9992212034847719419571082,
	    0.9992212034847719419571082, 0.9968860293500699344572167,
	    0.9929944970270556052085794, 0.9875442316181902420869293,
	    0.9805304560384748635197798, 0.9719459578929604660943619,
	    0.9617810305618204273733857, 0.9500233872907135082348609,
	    0.9366580463717067359685986, 0.9216671847173235372141197,
	    0.9050299562436422081151588, 0.8867222704506056271411144,
	    0.8667165253776934352758185, 0.8449812876768730110546447,
	    0.8214809108397780763011192, 0.7961750806125826782846465,
	    0.7690182743388618472684144, 0.7399591184920328886298624,
	    0.7089396263168475492411578, 0.6758942960988735968719870,
	    0.6407490519901669671243287, 0.6034200177680236694377441,
	    0.5638121389746919174509679, 0.5218177326736637589389453,
	    0.4773151991028322058895413, 0.4301685062972002231898738,
	    0.3802290003069655597718239, 0.3273435689467415081331061,
	    0.2713801935617592170277299, 0.2123038610981274593794731,
	    0.1504144244397940443049223, 0.0871941748480579986507079};
	static constexpr double table_high[2 * N] = {
	    0.1504144244397940443049223, 0.2123038610981274593794731,
	    0.2713801935617592170277299, 0.3273435689467415081331061,
	    0.3802290003069655597718239, 0.4301685062972002231898738,
	    0.4773151991028322058895413, 0.5218177326736637589389453,
	    0.5638121389746919174509679, 0.6034200177680236694377441,
	    0.6407490519901669671243287, 0.6758942960988735968719870,
	    0.7089396263168475492411578, 0.7399591184920328886298624,
	    0.7690182743388618472684144, 0.7961750806125826782846465,
	    0.8214809108397780763011192, 0.8449812876768730110546447,
	    0.8667165253776934352758185, 0.8867222704506056271411144,
	    0.9050299562436422081151588, 0.9216671847173235372141197,
	    0.9366580463717067359685986, 0.9500233872907135082348609,
	    0.9617810305618204273733857, 0.9719459578929604660943619,
	    0.9805304560384748635197798, 0.9875442316181902420869293,
	    0.9929944970270556052085794, 0.9968860293500699344572167,
	    0.9992212034847719419571082, 1.0000000000000000000000000,
	    1.0000000000000000000000000, 0.9992212034847719419571082,
	    0.9968860293500699344572167, 0.9929944970270556052085794,
	    0.9875442316181902420869293, 0.9805304560384748635197798,
	    0.9719459578929604660943619, 0.9617810305618204273733857,
	    0.9500233872907135082348609, 0.9366580463717067359685986,
	    0.9216671847173235372141197, 0.9050299562436422081151588,
	    0.8867222704506056271411144, 0.8667165253776934352758185,
	    0.8449812876768730110546447, 0.8214809108397780763011192,
	    0.7961750806125826782846465, 0.7690182743388618472684144,
	    0.7399591184920328886298624, 0.7089396263168475492411578,
	    0.6758942960988735968719870, 0.6407490519901669671243287,
	    0.6034200177680236694377441, 0.5638121389746919174509679,
	    0.5218177326736637589389453, 0.4773151991028322058895413,
	    0.4301685062972002231898738, 0.3802290003069655597718239,
	    0.3273435689467415081331061, 0.2713801935617592170277299,
	    0.2123038610981274593794731, 0.1504144244397940443049223};

  public:
	using result_type = double;

	/** constructors */
	truncated_normal_distribution()
	    : truncated_normal_distribution(0.0, 1.0, -1.0, 1.0)
	{}
	explicit truncated_normal_distribution(double mean, double stddev = 1.0,
	                                       double low = -1.0, double high = 1.0)
	    : mean_(mean), stddev_(stddev), low_((low - mean) / stddev),
	      high_((high - mean) / stddev), uniform(0.0, 1.0),
	      exponential(table_x[2 * N])
	{
		assert(stddev > 0.0);
		assert(low < high);
	}

	/** parameters */
	double mean() const { return mean_; }
	double stddev() const { return stddev_; }
	double low() const { return low_ * stddev_ + mean_; }
	double high() const { return high_ * stddev_ + mean_; }

	/** support of distribution */
	double min() const { return low(); }
	double max() const { return high(); }

	/** acceptance rate so far */
	double acceptance() const { return (double)nSamples / nTries; }

	/** (non-normalized) probability distribution function */
	double pdf(double x) const
	{
		x = (x - mean_) / stddev_;
		if (x < low_ || x > high_)
			return 0.0;
		return std::exp(-0.5 * x * x);
	}

	/** upper approximation of pdf(x) */
	double pdf_upper(double x) const
	{
		x = (x - mean_) / stddev_;
		if (x < low_ || x > high_)
			return 0.0;
		if (x < table_x[0])
			return std::exp(-x * table_x[0] + 0.5 * (table_x[0] * table_x[0]));
		if (x > table_x[2 * N])
			return std::exp(-x * table_x[2 * N] +
			                0.5 * (table_x[2 * N] * table_x[2 * N]));
		for (int reg = 0; reg < 2 * N; ++reg)
			if (table_x[reg + 1] >= x)
				return table_high[reg];
		return 0.0 / 0.0;
	}

	/** generate next variable */
	template <class Generator> result_type operator()(Generator &rng)
	{
		nSamples += 1;

		int reg_min = -1;
		int reg_max = 2 * N;
		while (table_x[reg_min + 1] <= low_)
			reg_min += 1;
		while (table_x[reg_max] >= high_)
			reg_max -= 1;

		while (true)
		{
			nTries += 1;
			int reg = std::uniform_int_distribution<int>(reg_min, reg_max)(rng);
			if (reg == -1 || reg == 2 * N)
			{
				double x = exponential(rng);

				if (uniform(rng) <= std::exp(-0.5 * x * x))
				{
					x += exponential.lambda();
					if (reg == -1)
						x = -x;
					if (x < low_ || x > high_)
						continue;
					return x * stddev_ + mean_;
				}
			}
			else
			{
				double x = table_x[reg] +
				           uniform(rng) * (table_x[reg + 1] - table_x[reg]);
				if (x < low_ || x > high_)
					continue;
				double y = uniform(rng) * table_high[reg];
				if (y <= table_low[reg] || y <= std::exp(-0.5 * x * x))
					return x * stddev_ + mean_;
			}
		}
	}
};

/**
 * Random numbers with "canonical quartic exponential" distribution
 * P(x) = const * exp(-x^4 - alpha*x^2 - beta*x)
 */
class canonical_quartic_exponential_distribution
{

	double alpha_ = 0.0, beta_ = 0.0; // parameters of the distribution
	int64_t nAccept = 0, nReject = 0; // statistics on the rejection-sampling

  public:
	using result_type = double;

	// constuctors
	canonical_quartic_exponential_distribution() = default;
	canonical_quartic_exponential_distribution(double alpha, double beta)
	    : alpha_(alpha), beta_(beta)
	{}

	// parameters
	double alpha() const { return alpha_; }
	double beta() const { return beta_; }

	// properties
	double min() const { return -std::numeric_limits<double>::infinity(); }
	double max() const { return std::numeric_limits<double>::infinity(); }
	double mean() const { return 0.0 / 0.0; }
	double variance() const { return 0.0 / 0.0; }
	double skewness() const { return 0.0 / 0.0; }
	double kurtosis() const { return 0.0 / 0.0; }

	// generator
	template <class Rng> result_type operator()(Rng &rng)
	{
		// this parameter is optimal in the case delta=0, for any gamma_
		double sigma =
		    0.5 * std::sqrt((std::sqrt(alpha_ * alpha_ + 4) - alpha_));
		double mu = -beta_ * sigma * sigma;

		// idea: sample a normal distribution with carefully chosen
		// parameters and do accept/reject to get precise distribution.
		double tmp = alpha_ - 1.0 / (2 * sigma * sigma); // gamma' in notes

		while (true)
		{
			double x = rng.normal() * mu + sigma;
			double p =
			    std::exp(-x * x * x * x - tmp * x * x - 0.25 * tmp * tmp);

			assert(p <= 1.0);
			if (rng.uniform() <= p)
			{
				nAccept += 1;
				return x;
			}
			else
				nReject += 1;
		}
	}

	/** non-normalized probability distribution function */
	double pdf(double x)
	{
		auto x2 = x * x;
		return std::exp(-x2 * x2 - alpha_ * x2 - beta_ * x);
	}

	/** acceptance rate so far (hopefully not much below 1.0) */
	double acceptance() const { return (double)nAccept / (nAccept + nReject); }
};

// Auto regressive model AR(p) providing the same interface as the distributions
template <size_t p> class Autoregressive
{
	normal_distribution noise_ = {0.0, 1.0};
	size_t pos = 0;
	std::array<double, p> hist_ = {};
	std::array<double, p> ws_ = {};

  public:
	using result_type = double;

	// constructors
	Autoregressive() = default;
	Autoregressive(std::array<double, p> const &ws) : ws_(ws) {}
	Autoregressive(std::array<double, p> const &ws,
	               normal_distribution const &noise)
	    : noise_(noise), ws_(ws)
	{}

	// parameters
	std::array<double, p> const &weights() const { return ws_; }

	// properties
	double min() const { return -std::numeric_limits<double>::infinity(); }
	double max() const { return std::numeric_limits<double>::infinity(); }
	double mean() const
	{
		double s = 0;
		for (size_t i = 0; i < p; ++i)
			s += ws_[i];
		return noise_.mean() / (1 - s);
	}
	double variance() const
	{
		if (p == 0)
			return noise_.variance();
		else if (p == 1)
			return noise_.variance() / (1 - ws_[0] * ws_[0]);
		else
			return 0.0 / 0.0;
		// TODO: should be easy enough to derive the general expression
		//       (maybe also kurtosis?)
	}
	double skewness() const { return 0.0; }
	double kurtosis() const { return 0.0 / 0.0; }

	// generator
	template <class Rng> result_type operator()(Rng &rng)
	{
		double r = noise_(rng);
		for (size_t i = 0; i < p; ++i)
			r += ws_[i] * hist_[(pos - i - 1 + p) % p];
		hist_[(pos++) % p] = r;
		return r;
	}
};

// deduction guideline
template <size_t N> Autoregressive(double const (&ws)[N]) -> Autoregressive<N>;
template <size_t N>
Autoregressive(double const (&ws)[N], normal_distribution const &)
    -> Autoregressive<N>;

} // namespace util
