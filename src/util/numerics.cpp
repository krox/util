#include "util/numerics.h"

#include <cassert>
#include <cmath>
#include <queue>
using namespace std;

namespace util {

using std::isnan, std::abs, std::min, std::max;

double solve(function_t f, double a, double b)
{
	assert(!isnan(a) && !isnan(b));

	double fa = f(a);
	double fb = f(b);
	assert(!std::isnan(fa) && !isnan(fb));
	if (fa == 0)
		return a;
	if (fb == 0)
		return b;
	assert(signbit(fa) != signbit(fb));
	double c = b;
	double fc = fb;

	// a should be the best guess
	if (abs(fb) < abs(fa))
	{
		swap(a, b);
		swap(fa, fb);
	}

	for (int iter = 0; iter < 100; ++iter)
	{
		// choose new point x
		double x = (b * fa - a * fb) / (fa - fb); // secant method

		// outside bracket (or nan) -> fall back to bisection
		if (!(a < x && x < c) && !(c < x && x < a))
		{
			x = 0.5 * (a + c);
			if (x == a || x == c) // there is no further floating point number
			                      // between a and b -> we are done
			{
				if (abs(fc) < abs(fa))
					return c;
				else
					return a;
			}
		}

		// evaluate f at new point
		b = a;
		fb = fa;
		a = x;
		fa = f(x);
		if (isnan(fa))
			throw numerics_exception(
			    "ERROR: function gave NaN during secant method");
		if (fa == 0)
			return a;

		// update brackets
		if (signbit(fa) != signbit(fb))
		{
			c = b;
			fc = fb;
		}
	}

	throw numerics_exception("secant method did not converge");
}

namespace {
// Gauss/Kronrod nodes
static const double GK31_x[] = {
    0.000000000000000000000000000000000e+00,
    2.011940939974345223006283033945962e-01,
    3.941513470775633698972073709810455e-01,
    5.709721726085388475372267372539106e-01,
    7.244177313601700474161860546139380e-01,
    8.482065834104272162006483207742169e-01,
    9.372733924007059043077589477102095e-01,
    9.879925180204854284895657185866126e-01,

    1.011420669187174990270742314473923e-01,
    2.991800071531688121667800242663890e-01,
    4.850818636402396806936557402323506e-01,
    6.509967412974169705337358953132747e-01,
    7.904185014424659329676492948179473e-01,
    8.972645323440819008825096564544959e-01,
    9.677390756791391342573479787843372e-01,
    9.980022986933970602851728401522712e-01,
};

// Gauss weights
static const double GK31_wg[] = {
    2.025782419255612728806201999675193e-01,
    1.984314853271115764561183264438393e-01,
    1.861610000155622110268005618664228e-01,
    1.662692058169939335532008604812088e-01,
    1.395706779261543144478047945110283e-01,
    1.071592204671719350118695466858693e-01,
    7.036604748810812470926741645066734e-02,
    3.075324199611726835462839357720442e-02,
};

// Kronrod weights
static const double GK31_wk[] = {
    1.013300070147915490173747927674925e-01,
    9.917359872179195933239317348460313e-02,
    9.312659817082532122548687274734572e-02,
    8.308050282313302103828924728610379e-02,
    6.985412131872825870952007709914748e-02,
    5.348152469092808726534314723943030e-02,
    3.534636079137584622203794847836005e-02,
    1.500794732931612253837476307580727e-02,

    1.007698455238755950449466626175697e-01,
    9.664272698362367850517990762758934e-02,
    8.856444305621177064727544369377430e-02,
    7.684968075772037889443277748265901e-02,
    6.200956780067064028513923096080293e-02,
    4.458975132476487660822729937327969e-02,
    2.546084732671532018687400101965336e-02,
    5.377479872923348987792051430127650e-03,
};

/** returns (Gauss,Kronrod) quadrature using 15/31 function evaluations */
static pair<double, double> integrateKronrod31(function_t f, double a, double b)
{
	double mid = (a + b) / 2;
	double half = (b - a) / 2;

	double f0 = f(mid);
	double sumG = GK31_wg[0] * f0;
	double sumK = GK31_wk[0] * f0;
	for (size_t i = 1; i < 8; ++i)
	{
		f0 = f(mid - half * GK31_x[i]) + f(mid + half * GK31_x[i]);
		sumG += GK31_wg[i] * f0;
		sumK += GK31_wk[i] * f0;
	}
	for (size_t i = 8; i < 16; ++i)
	{
		f0 = f(mid - half * GK31_x[i]) + f(mid + half * GK31_x[i]);
		sumK += GK31_wk[i] * f0;
	}

	sumG *= half;
	sumK *= half;
	return {sumG, sumK};
}

struct Region
{
	double a, b;
	double val, err;

	Region(function_t f, double a, double b) : a(a), b(b)
	{
		auto est = integrateKronrod31(f, a, b);
		val = est.second;
		err = abs(est.first - est.second);
	}

	bool operator<(const Region &b) const { return err < b.err; }
};

} // namespace

double integrate(function_t f, double a, double b, double eps, int maxCalls)
{
	priority_queue<Region> q;

	auto reg = Region(f, a, b);
	double val = reg.val;
	double err = reg.err;
	q.push(reg);

	while (abs(err / val) > eps)
	{
		if (31 * (int)q.size() >= maxCalls)
			throw numerics_exception(
			    "Gauss-Kronrod adaptive integral did not converge.");

		reg = q.top();
		q.pop();
		auto regLeft = Region(f, reg.a, 0.5 * (reg.a + reg.b));
		auto regRight = Region(f, 0.5 * (reg.a + reg.b), reg.b);
		val += regLeft.val + regRight.val - reg.val;
		err += regLeft.err + regRight.err - reg.err;
		q.push(regLeft);
		q.push(regRight);
	}
	return val;
}

double integrate(function_t f, double a, double b)
{
	return integrate(f, a, b, 1.0e-12, 5000);
}

namespace {
// Gauss-Hermite 15/31/63-point rules for w(x) = e^(-x^2)
static const double GH15_x[] = {0.000000000000000000, 0.565069583255575749,
                                1.136115585210920666, 1.719992575186488932,
                                2.325732486173857745, 2.967166927905603248,
                                3.669950373404452535, 4.499990707309391554};
static const double GH15_w[] = {0.564100308726417533, 0.567021153446603929,
                                0.576193350283499650, 0.593027449764209533,
                                0.620662603527037137, 0.666166005109043837,
                                0.748607366016906250, 0.948368970827605186};
static const double GH31_x[] = {
    0.000000000000000000, 0.395942736471423111, 0.792876976915308940,
    1.191826998350046426, 1.593885860472139826, 2.000258548935638966,
    2.412317705480420105, 2.831680453390205456, 3.260320732313540810,
    3.700743403231469422, 4.156271755818145172, 4.631559506312859942,
    5.133595577112380705, 5.673961444618588330, 6.275078704942860143,
    6.995680123718540275};
static const double GH31_w[] = {
    0.395778556098609545, 0.396271628748323050, 0.397766973762304700,
    0.400314539104558889, 0.404003106480250207, 0.408969795872926790,
    0.415416223407638680, 0.423635472285710648, 0.434058004539506410,
    0.447333228420965560, 0.464483790760098421, 0.487223525683032566,
    0.518694458546355664, 0.565491088874326346, 0.644938481717208134,
    0.829310817431187715};

static const double GH63_x[] = {
    0.000000000000000000, 0.278795385671152239, 0.557761664279082216,
    0.837071095589476159, 1.116898705099646269, 1.397423748604962510,
    1.678831279172013752, 1.961313858308148529, 2.245073460481206629,
    2.530323630471201092, 2.817291967283797775, 3.106223027928256632,
    3.397381771330391185, 3.691057700096346511, 3.987569910419715748,
    4.287273335282440403, 4.590566574443519022, 4.897901864497574235,
    5.209797983040835486, 5.526857252640303142, 5.849788400081067346,
    6.179437992270596986, 6.516834810682116060, 6.863254433179536852,
    7.220316707888967846, 7.590139519864106676, 7.975595080142037318,
    8.380768345186321934, 8.811858143728454644, 9.279201954305039131,
    9.802875991297496363, 10.43549987785416805};
static const double GH63_w[] = {
    0.278766948849251654, 0.27885228194447375, 0.27910896199662452,
    0.27953904721768528,  0.28014602175092568, 0.28093487903385774,
    0.28191224408290622,  0.28308654081828920, 0.28446821314522270,
    0.28607001188838763,  0.28790736420948081, 0.28999884836314213,
    0.29236680537646796,  0.29503813171828595, 0.29804531521221171,
    0.30142780347986625,  0.30523383523220673, 0.30952292844871419,
    0.31436932097542589,  0.31986682534634250, 0.32613584085167684,
    0.33333375907856675,  0.34167090105582781, 0.35143585583314425,
    0.36303761271758226,  0.37707955956817546, 0.39449866004677920,
    0.41685139846476949,  0.44697543147121176, 0.49080461082555881,
    0.56388743665962956,  0.73094557374600897};
} // namespace

double integrate_hermite_15(function_t f)
{
	double s = GH15_w[0] * f(0);
	for (size_t i = 1; i < 8; ++i)
		s += GH15_w[i] * (f(-GH15_x[i]) + f(GH15_x[i]));
	return s;
}

double integrate_hermite_31(function_t f)
{
	double s = GH31_w[0] * f(0);
	for (size_t i = 1; i < 16; ++i)
		s += GH31_w[i] * (f(-GH31_x[i]) + f(GH31_x[i]));
	return s;
}

double integrate_hermite_63(function_t f)
{
	double s = GH63_w[0] * f(0);
	for (size_t i = 1; i < 32; ++i)
		s += GH63_w[i] * (f(-GH63_x[i]) + f(GH63_x[i]));
	return s;
}

FSum::FSum(double x)
{
	if (x != 0)
		parts_.push_back(x);
}

FSum &FSum::operator+=(double x)
{
	// NOTE: this algorithm ensures that the parts have non-overlapping
	// bits, but it does not ensure that the mantissas are fully utilized.
	// I.e. it can happen that each part only contains a single bit.
	size_t j = 0;
	for (double y : parts_)
	{
		double high = x + y;
		double tmp = high - x;
		double low = (x - (high - tmp)) + (y - tmp);

		if (low != 0.0)
			parts_[j++] = low;
		x = high;
	}
	parts_.resize(j);
	if (x != 0)
		parts_.push_back(x);
	return *this;
}

FSum &FSum::operator-=(double x) { return *this += -x; }

FSum::operator double() const
{
	// this sums low to high. correctnes proof in
	//    www-2.cs.cmu.edu/afs/cs/project/quake/public/papers/robust-arithmetic.ps
	// could be optimized by summing high to low and stopping when sum
	// becomes inexact. Though that needs some annoying fixup to make
	// round-to-even work correctly accros multiple partials.
	double total = 0;
	for (double x : parts_)
		total += x;
	return total;
}

} // namespace util
