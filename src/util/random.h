#ifndef UTIL_RANDOM_H
#define UTIL_RANDOM_H

/**
 * Pseudorandom number generators.
 */

#include <random>
#include <stdint.h>

namespace util {

/**
 * Originally written in 2015 by Sebastiano Vigna (vigna@acm.org).
 * public domain, taken from http://xoroshiro.di.unimi.it/splitmix64.c
 */
class splitmix64
{
	uint64_t s; // all values are allowed

  public:
	splitmix64() : s(0) {}
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
 * public domain, from http://http://xoshiro.di.unimi.it/xoshiro256starstar.c
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

	using result_type = uint64_t;
	uint64_t min() const { return 0; }
	uint64_t max() const { return UINT64_MAX; }

	/** set the internal state to some seed value */
	void seed(uint64_t x)
	{
		splitmix64 gen(x);
		s[0] = gen();
		s[1] = gen();
		s[2] = gen();
		s[3] = gen();
	}

	/** generate next value in the random sequence */
	uint64_t operator()()
	{
		uint64_t result = rotl(s[1] * 5, 7) * 9;

		uint64_t t = s[1] << 17;
		s[2] ^= s[0];
		s[3] ^= s[1];
		s[1] ^= s[2];
		s[0] ^= s[3];
		s[2] ^= t;
		s[3] = rotl(s[3], 45);

		return result;
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

template <class RealType = double> class truncated_normal_distribution
{
	// TODO: performance can break down if the two limits are close together or
	//       if sampling far into the tail. Fallback could be a simple
	//       exponential proposal

	// parameters of the distribution
	RealType mean_, stddev_;
	RealType low_, high_; // normalized to mean/stddev

	std::uniform_real_distribution<RealType> uniform;
	std::exponential_distribution<RealType> exponential;

	// performance statistics
	int64_t nSamples = 0, nTries = 0;

	// (x,f(x)) pairs of f(x)=e^(-x^2/2), such that the 2*N+2 upper
	// approximations (2N x rectangles + 2 x exponential) have the same area.
	// should yield ~96% acceptance (~90% without evaluating f)
	static constexpr int N = 32;
	static constexpr RealType table_x[2 * N + 1] = {
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
	static constexpr RealType table_low[2 * N] = {
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
	static constexpr RealType table_high[2 * N] = {
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
	using result_type = RealType;

	/** constructors */
	truncated_normal_distribution()
	    : truncated_normal_distribution(0.0, 1.0, -1.0, 1.0)
	{}
	explicit truncated_normal_distribution(RealType mean, RealType stddev = 1.0,
	                                       RealType low = -1.0,
	                                       RealType high = 1.0)
	    : mean_(mean), stddev_(stddev), low_((low - mean) / stddev),
	      high_((high - mean) / stddev), uniform(0.0, 1.0),
	      exponential(table_x[2 * N])
	{
		assert(stddev > 0.0);
		assert(low < high);
	}

	/** parameters */
	RealType mean() const { return mean_; }
	RealType stddev() const { return stddev_; }
	RealType low() const { return low_ * stddev_ + mean_; }
	RealType high() const { return high_ * stddev_ + mean_; }

	/** support of distribution */
	RealType min() const { return low(); }
	RealType max() const { return high(); }

	/** acceptance rate so far */
	double acceptance() const { return (double)nSamples / nTries; }

	/** (non-normalized) probability distribution function */
	RealType pdf(RealType x) const
	{
		x = (x - mean_) / stddev_;
		if (x < low_ || x > high_)
			return 0.0;
		return std::exp(-0.5 * x * x);
	}

	/** upper approximation of pdf(x) */
	RealType pdf_upper(RealType x) const
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
				RealType x = exponential(rng);

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
				RealType x = table_x[reg] +
				             uniform(rng) * (table_x[reg + 1] - table_x[reg]);
				if (x < low_ || x > high_)
					continue;
				RealType y = uniform(rng) * table_high[reg];
				if (y <= table_low[reg] || y <= std::exp(-0.5 * x * x))
					return x * stddev_ + mean_;
			}
		}
	}
};

} // namespace util

#endif
