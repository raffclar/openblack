/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The steps of this file come from fdlibm (Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved. Developed at
// SunPro, a Sun Microsystems, Inc. business. Permission to use, copy, modify, and distribute this software is freely
// granted, provided that this notice is preserved.), as musl keeps them (MIT licence).

#include "PortableMath.h"

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>
#include <limits>
#include <optional>

#include "PortableMathTables.h"

namespace openblack::portable_math
{

namespace
{

[[nodiscard]] uint32_t HighWord(double value)
{
	return static_cast<uint32_t>(std::bit_cast<uint64_t>(value) >> 32);
}

[[nodiscard]] uint32_t LowWord(double value)
{
	return static_cast<uint32_t>(std::bit_cast<uint64_t>(value));
}

[[nodiscard]] double WithLowWordZero(double value)
{
	return std::bit_cast<double>(std::bit_cast<uint64_t>(value) & 0xFFFF'FFFF'0000'0000ull);
}

[[nodiscard]] double Abs(double value)
{
	return std::bit_cast<double>(std::bit_cast<uint64_t>(value) & 0x7FFF'FFFF'FFFF'FFFFull);
}

[[nodiscard]] bool IsNan(double value)
{
	return (std::bit_cast<uint64_t>(value) & 0x7FFF'FFFF'FFFF'FFFFull) > 0x7FF0'0000'0000'0000ull;
}

/// The sine of a reduced angle x + y, |x + y| at most pi / 4. With tail false, y is 0.
[[nodiscard]] double SinKernel(double x, double y, bool tail)
{
	constexpr double k_S1 = -1.66666666666666324348e-01;
	constexpr double k_S2 = 8.33333333332248946124e-03;
	constexpr double k_S3 = -1.98412698298579493134e-04;
	constexpr double k_S4 = 2.75573137070700676789e-06;
	constexpr double k_S5 = -2.50507602534068634195e-08;
	constexpr double k_S6 = 1.58969099521155010221e-10;

	const double z = x * x;
	const double w = z * z;
	const double r = k_S2 + z * (k_S3 + z * k_S4) + z * w * (k_S5 + z * k_S6);
	const double v = z * x;
	if (!tail)
	{
		return x + v * (k_S1 + z * r);
	}
	return x - ((z * (0.5 * y - v * r) - y) - v * k_S1);
}

/// The cosine of a reduced angle x + y, |x + y| at most pi / 4
[[nodiscard]] double CosKernel(double x, double y)
{
	constexpr double k_C1 = 4.16666666666666019037e-02;
	constexpr double k_C2 = -1.38888888888741095749e-03;
	constexpr double k_C3 = 2.48015872894767294178e-05;
	constexpr double k_C4 = -2.75573143513906633035e-07;
	constexpr double k_C5 = 2.08757232129817482790e-09;
	constexpr double k_C6 = -1.13596475577881948265e-11;

	const double z = x * x;
	double w = z * z;
	const double r = z * (k_C1 + z * (k_C2 + z * k_C3)) + w * w * (k_C4 + z * (k_C5 + z * k_C6));
	const double hz = 0.5 * z;
	w = 1.0 - hz;
	return w + (((1.0 - w) - hz) + (z * r - x * y));
}

/// The tangent of a reduced angle x + y, |x + y| at most pi / 4, or minus its inverse when odd
[[nodiscard]] double TanKernel(double x, double y, bool odd)
{
	constexpr std::array<double, 13> k_T = {
	    3.33333333333334091986e-01, 1.33333333333201242699e-01, 5.39682539762260521377e-02, 2.18694882948595424599e-02,
	    8.86323982359930005737e-03, 3.59207910759131235356e-03, 1.45620945432529025516e-03, 5.88041240820264096874e-04,
	    2.46463134818469906812e-04, 7.81794442939557092300e-05, 7.14072491382608190305e-05, -1.85586374855275456654e-05,
	    2.59073051863633712884e-05,
	};
	constexpr double k_Pio4 = 7.85398163397448278999e-01;
	constexpr double k_Pio4Lo = 3.06161699786838301793e-17;

	const uint32_t hx = HighWord(x);
	const bool big = (hx & 0x7FFF'FFFFu) >= 0x3FE5'9428u;
	bool negative = false;
	if (big)
	{
		negative = (hx >> 31) != 0;
		if (negative)
		{
			x = -x;
			y = -y;
		}
		x = (k_Pio4 - x) + (k_Pio4Lo - y);
		y = 0.0;
	}
	const double z = x * x;
	double w = z * z;
	double r = k_T[1] + w * (k_T[3] + w * (k_T[5] + w * (k_T[7] + w * (k_T[9] + w * k_T[11]))));
	double v = z * (k_T[2] + w * (k_T[4] + w * (k_T[6] + w * (k_T[8] + w * (k_T[10] + w * k_T[12])))));
	double s = z * x;
	r = y + z * (s * (r + v) + y) + s * k_T[0];
	w = x + r;
	if (big)
	{
		s = odd ? -1.0 : 1.0;
		v = s - 2.0 * (x + (r - w * w / (w + s)));
		return negative ? -v : v;
	}
	if (!odd)
	{
		return w;
	}
	// -1 / (x + r) has an error of up to 2 units in the last place, so it is made in two parts
	const double w0 = WithLowWordZero(w);
	v = r - (w0 - x);
	const double a = -1.0 / w;
	const double a0 = WithLowWordZero(a);
	return a0 + a * (1.0 + a0 * w0 + a0 * v);
}

/// An angle less the whole number of quarter turns nearest to it: the number of quarter turns, and the rest as an exact
/// sum of two doubles. The caller does the angles of at most pi / 4 itself.
struct Reduced
{
	int64_t quarters;
	double head;
	double tail;
};

[[nodiscard]] Reduced ReduceThreeParts(double x, uint32_t ix)
{
	constexpr double k_ToInt = 1.5 / std::numeric_limits<double>::epsilon();
	constexpr double k_Pio4 = 0x1.921fb54442d18p-1;
	constexpr double k_InvPio2 = 6.36619772367581382433e-01;
	constexpr double k_Pio2Part1 = 1.57079632673412561417e+00;
	constexpr double k_Pio2Part1Tail = 6.07710050650619224932e-11;
	constexpr double k_Pio2Part2 = 6.07710050630396597660e-11;
	constexpr double k_Pio2Part2Tail = 2.02226624879595063154e-21;
	constexpr double k_Pio2Part3 = 2.02226624871116645580e-21;
	constexpr double k_Pio2Part3Tail = 8.47842766036889956997e-32;

	// The nearest whole number of x / (pi / 2)
	double fn = (x * k_InvPio2 + k_ToInt) - k_ToInt;
	auto n = static_cast<int64_t>(fn);
	double r = x - fn * k_Pio2Part1;
	double w = fn * k_Pio2Part1Tail;
	if (r - w < -k_Pio4)
	{
		--n;
		fn -= 1.0;
		r = x - fn * k_Pio2Part1;
		w = fn * k_Pio2Part1Tail;
	}
	else if (r - w > k_Pio4)
	{
		++n;
		fn += 1.0;
		r = x - fn * k_Pio2Part1;
		w = fn * k_Pio2Part1Tail;
	}
	double head = r - w;
	const auto exponent = static_cast<int>(ix >> 20);
	if (exponent - static_cast<int>((HighWord(head) >> 20) & 0x7FFu) > 16)
	{
		// The second part, good to 118 bits
		double t = r;
		w = fn * k_Pio2Part2;
		r = t - w;
		w = fn * k_Pio2Part2Tail - ((t - r) - w);
		head = r - w;
		if (exponent - static_cast<int>((HighWord(head) >> 20) & 0x7FFu) > 49)
		{
			// The third part, good to 151 bits
			t = r;
			w = fn * k_Pio2Part3;
			r = t - w;
			w = fn * k_Pio2Part3Tail - ((t - r) - w);
			head = r - w;
		}
	}
	return {.quarters = n, .head = head, .tail = (r - head) - w};
}

[[nodiscard]] Reduced Reduce(double x)
{
	constexpr double k_Pio2Part1 = 1.57079632673412561417e+00;
	constexpr double k_Pio2Part1Tail = 6.07710050650619224932e-11;

	const uint32_t ix = HighWord(x) & 0x7FFF'FFFFu;
	const bool negative = (HighWord(x) >> 31) != 0;
	// One, two, three or four quarter turns, away from the angles where the subtraction cancels many bits
	const auto near = [x, negative](int quarters) {
		const double sign = negative ? -1.0 : 1.0;
		const double whole = static_cast<double>(quarters);
		const double z = x - sign * whole * k_Pio2Part1;
		const double head = z - sign * whole * k_Pio2Part1Tail;
		const double tail = (z - head) - sign * whole * k_Pio2Part1Tail;
		return Reduced {.quarters = negative ? -quarters : quarters, .head = head, .tail = tail};
	};
	if (ix <= 0x400F'6A7Au)
	{
		if ((ix & 0xF'FFFFu) == 0x9'21FBu)
		{
			return ReduceThreeParts(x, ix);
		}
		return near(ix <= 0x4002'D97Cu ? 1 : 2);
	}
	if (ix <= 0x401C'463Bu)
	{
		if (ix <= 0x4015'FDBCu)
		{
			if (ix == 0x4012'D97Cu)
			{
				return ReduceThreeParts(x, ix);
			}
			return near(3);
		}
		if (ix == 0x4019'21FBu)
		{
			return ReduceThreeParts(x, ix);
		}
		return near(4);
	}
	return ReduceThreeParts(x, ix);
}

} // namespace

double Sin(double radians)
{
	const uint32_t ix = HighWord(radians) & 0x7FFF'FFFFu;
	if (ix <= 0x3FE9'21FBu)
	{
		if (ix < 0x3E50'0000u)
		{
			return radians;
		}
		return SinKernel(radians, 0.0, false);
	}
	if (ix >= 0x7FF0'0000u)
	{
		return radians - radians;
	}
	const auto reduced = Reduce(radians);
	switch (reduced.quarters & 3)
	{
	case 0:
		return SinKernel(reduced.head, reduced.tail, true);
	case 1:
		return CosKernel(reduced.head, reduced.tail);
	case 2:
		return -SinKernel(reduced.head, reduced.tail, true);
	default:
		return -CosKernel(reduced.head, reduced.tail);
	}
}

double Cos(double radians)
{
	const uint32_t ix = HighWord(radians) & 0x7FFF'FFFFu;
	if (ix <= 0x3FE9'21FBu)
	{
		if (ix < 0x3E46'A09Eu)
		{
			return 1.0;
		}
		return CosKernel(radians, 0.0);
	}
	if (ix >= 0x7FF0'0000u)
	{
		return radians - radians;
	}
	const auto reduced = Reduce(radians);
	switch (reduced.quarters & 3)
	{
	case 0:
		return CosKernel(reduced.head, reduced.tail);
	case 1:
		return -SinKernel(reduced.head, reduced.tail, true);
	case 2:
		return -CosKernel(reduced.head, reduced.tail);
	default:
		return SinKernel(reduced.head, reduced.tail, true);
	}
}

double Tan(double radians)
{
	const uint32_t ix = HighWord(radians) & 0x7FFF'FFFFu;
	if (ix <= 0x3FE9'21FBu)
	{
		if (ix < 0x3E40'0000u)
		{
			return radians;
		}
		return TanKernel(radians, 0.0, false);
	}
	if (ix >= 0x7FF0'0000u)
	{
		return radians - radians;
	}
	const auto reduced = Reduce(radians);
	return TanKernel(reduced.head, reduced.tail, (reduced.quarters & 1) != 0);
}

double Atan(double value)
{
	constexpr std::array<double, 4> k_AtanHigh = {
	    4.63647609000806093515e-01,
	    7.85398163397448278999e-01,
	    9.82793723247329054082e-01,
	    1.57079632679489655800e+00,
	};
	constexpr std::array<double, 4> k_AtanLow = {
	    2.26987774529616870924e-17,
	    3.06161699786838301793e-17,
	    1.39033110312309984516e-17,
	    6.12323399573676603587e-17,
	};
	constexpr std::array<double, 11> k_A = {
	    3.33333333333329318027e-01, -1.99999999998764832476e-01, 1.42857142725034663711e-01, -1.11111104054623557880e-01,
	    9.09088713343650656196e-02, -7.69187620504482999495e-02, 6.66107313738753120669e-02, -5.83357013379057348645e-02,
	    4.97687799461593236017e-02, -3.65315727442169155270e-02, 1.62858201153657823623e-02,
	};

	uint32_t ix = HighWord(value);
	const bool negative = (ix >> 31) != 0;
	ix &= 0x7FFF'FFFFu;
	double x = value;
	int id = -1;
	if (ix >= 0x4410'0000u)
	{
		// |x| of 2^66 or more
		if (IsNan(x))
		{
			return x;
		}
		return negative ? -k_AtanHigh[3] : k_AtanHigh[3];
	}
	if (ix < 0x3FDC'0000u)
	{
		// |x| less than 0.4375
		if (ix < 0x3E40'0000u)
		{
			return x;
		}
	}
	else
	{
		x = Abs(x);
		if (ix < 0x3FF3'0000u)
		{
			if (ix < 0x3FE6'0000u)
			{
				id = 0;
				x = (2.0 * x - 1.0) / (2.0 + x);
			}
			else
			{
				id = 1;
				x = (x - 1.0) / (x + 1.0);
			}
		}
		else if (ix < 0x4003'8000u)
		{
			id = 2;
			x = (x - 1.5) / (1.0 + 1.5 * x);
		}
		else
		{
			id = 3;
			x = -1.0 / x;
		}
	}
	double z = x * x;
	const double w = z * z;
	const double s1 = z * (k_A[0] + w * (k_A[2] + w * (k_A[4] + w * (k_A[6] + w * (k_A[8] + w * k_A[10])))));
	const double s2 = w * (k_A[1] + w * (k_A[3] + w * (k_A[5] + w * (k_A[7] + w * k_A[9]))));
	if (id < 0)
	{
		return x - x * (s1 + s2);
	}
	const auto at = static_cast<size_t>(id);
	z = k_AtanHigh[at] - (x * (s1 + s2) - k_AtanLow[at] - x);
	return negative ? -z : z;
}

double Atan2(double y, double x)
{
	constexpr double k_Pi = 3.1415926535897931160E+00;
	constexpr double k_PiLow = 1.2246467991473531772E-16;

	if (IsNan(x) || IsNan(y))
	{
		return x + y;
	}
	uint32_t ix = HighWord(x);
	const uint32_t lx = LowWord(x);
	uint32_t iy = HighWord(y);
	const uint32_t ly = LowWord(y);
	if (((ix - 0x3FF0'0000u) | lx) == 0)
	{
		return Atan(y);
	}
	// Twice the sign of x and the sign of y
	const uint32_t m = ((iy >> 31) & 1u) | ((ix >> 30) & 2u);
	ix &= 0x7FFF'FFFFu;
	iy &= 0x7FFF'FFFFu;
	if ((iy | ly) == 0)
	{
		switch (m)
		{
		case 0:
		case 1:
			return y;
		case 2:
			return k_Pi;
		default:
			return -k_Pi;
		}
	}
	if ((ix | lx) == 0)
	{
		return (m & 1u) != 0 ? -k_Pi / 2 : k_Pi / 2;
	}
	if (ix == 0x7FF0'0000u)
	{
		if (iy == 0x7FF0'0000u)
		{
			switch (m)
			{
			case 0:
				return k_Pi / 4;
			case 1:
				return -k_Pi / 4;
			case 2:
				return 3 * k_Pi / 4;
			default:
				return -3 * k_Pi / 4;
			}
		}
		switch (m)
		{
		case 0:
			return 0.0;
		case 1:
			return -0.0;
		case 2:
			return k_Pi;
		default:
			return -k_Pi;
		}
	}
	if (ix + (64u << 20) < iy || iy == 0x7FF0'0000u)
	{
		return (m & 1u) != 0 ? -k_Pi / 2 : k_Pi / 2;
	}
	double z = 0.0;
	if ((m & 2u) == 0 || iy + (64u << 20) >= ix)
	{
		z = Atan(Abs(y / x));
	}
	switch (m)
	{
	case 0:
		return z;
	case 1:
		return -z;
	case 2:
		return k_Pi - (z - k_PiLow);
	default:
		return (z - k_PiLow) - k_Pi;
	}
}

double Acos(double value)
{
	constexpr double k_Pio2Hi = 1.57079632679489655800e+00;
	constexpr double k_Pio2Lo = 6.12323399573676603587e-17;
	constexpr double k_PS0 = 1.66666666666666657415e-01;
	constexpr double k_PS1 = -3.25565818622400915405e-01;
	constexpr double k_PS2 = 2.01212532134862925881e-01;
	constexpr double k_PS3 = -4.00555345006794114027e-02;
	constexpr double k_PS4 = 7.91534994289814532176e-04;
	constexpr double k_PS5 = 3.47933107596021167570e-05;
	constexpr double k_QS1 = -2.40339491173441421878e+00;
	constexpr double k_QS2 = 2.02094576023350569471e+00;
	constexpr double k_QS3 = -6.88283971605453293030e-01;
	constexpr double k_QS4 = 7.70381505559019352791e-02;
	const auto rational = [](double z) {
		const double p = z * (k_PS0 + z * (k_PS1 + z * (k_PS2 + z * (k_PS3 + z * (k_PS4 + z * k_PS5)))));
		const double q = 1.0 + z * (k_QS1 + z * (k_QS2 + z * (k_QS3 + z * k_QS4)));
		return p / q;
	};

	const double x = value;
	const uint32_t hx = HighWord(x);
	const uint32_t ix = hx & 0x7FFF'FFFFu;
	if (ix >= 0x3FF0'0000u)
	{
		// |x| of 1 or more, or not a number
		if (((ix - 0x3FF0'0000u) | LowWord(x)) == 0)
		{
			return (hx >> 31) != 0 ? 2 * k_Pio2Hi : 0.0;
		}
		return std::numeric_limits<double>::quiet_NaN();
	}
	if (ix < 0x3FE0'0000u)
	{
		// |x| less than 0.5
		if (ix <= 0x3C60'0000u)
		{
			return k_Pio2Hi;
		}
		return k_Pio2Hi - (x - (k_Pio2Lo - x * rational(x * x)));
	}
	if ((hx >> 31) != 0)
	{
		// x less than -0.5
		const double z = (1.0 + x) * 0.5;
		const double s = std::sqrt(z);
		const double w = rational(z) * s - k_Pio2Lo;
		return 2 * (k_Pio2Hi - (s + w));
	}
	// x more than 0.5
	const double z = (1.0 - x) * 0.5;
	const double s = std::sqrt(z);
	const double df = WithLowWordZero(s);
	const double c = (z - df * df) / (s + df);
	const double w = rational(z) * s + c;
	return 2 * (df + w);
}

double Exp(double value)
{
	namespace table = tables::exp;
	constexpr uint32_t k_N = 1u << table::k_TableBits;
	const auto top12 = [](double x) { return static_cast<uint32_t>(std::bit_cast<uint64_t>(x) >> 52); };

	const double x = value;
	uint32_t abstop = top12(x) & 0x7FFu;
	if (abstop - top12(0x1p-54) >= top12(512.0) - top12(0x1p-54))
	{
		if (abstop - top12(0x1p-54) >= 0x8000'0000u)
		{
			// A tiny x
			return 1.0 + x;
		}
		if (abstop >= top12(1024.0))
		{
			if (std::bit_cast<uint64_t>(x) == std::bit_cast<uint64_t>(-std::numeric_limits<double>::infinity()))
			{
				return 0.0;
			}
			if (abstop >= top12(std::numeric_limits<double>::infinity()))
			{
				return 1.0 + x;
			}
			return (std::bit_cast<uint64_t>(x) >> 63) != 0 ? 0.0 : std::numeric_limits<double>::infinity();
		}
		// A large x goes through the special case below
		abstop = 0;
	}
	// exp(x) = 2^(k/N) * exp(r), with x = ln2/N * k + r
	const double z = table::k_InvLn2N * x;
	double kd = z + table::k_Shift;
	const uint64_t ki = std::bit_cast<uint64_t>(kd);
	kd -= table::k_Shift;
	const double r = x + kd * table::k_NegLn2HiN + kd * table::k_NegLn2LoN;
	const auto idx = static_cast<size_t>(2 * (ki % k_N));
	const uint64_t top = ki << (52 - table::k_TableBits);
	const double tail = std::bit_cast<double>(table::k_Tab[idx]);
	uint64_t sbits = table::k_Tab[idx + 1] + top;
	const double r2 = r * r;
	const auto& c = table::k_Poly;
	const double tmp = tail + r + r2 * (c[0] + r * c[1]) + r2 * r2 * (c[2] + r * c[3]);
	if (abstop == 0)
	{
		// The result can overflow or underflow
		if ((ki & 0x8000'0000u) == 0)
		{
			sbits -= 1009ull << 52;
			const double scale = std::bit_cast<double>(sbits);
			return 0x1p1009 * (scale + scale * tmp);
		}
		sbits += 1022ull << 52;
		const double scale = std::bit_cast<double>(sbits);
		double y = scale + scale * tmp;
		if (y < 1.0)
		{
			double lo = scale - y + scale * tmp;
			const double hi = 1.0 + y;
			lo = 1.0 - hi + y + lo;
			y = (hi + lo) - 1.0;
			if (y == 0.0)
			{
				y = 0.0;
			}
		}
		return 0x1p-1022 * y;
	}
	const double scale = std::bit_cast<double>(sbits);
	return scale + scale * tmp;
}

double Expm1(double value)
{
	constexpr double k_OverflowAbove = 7.09782712893383973096e+02;
	constexpr double k_Ln2Hi = 6.93147180369123816490e-01;
	constexpr double k_Ln2Lo = 1.90821492927058770002e-10;
	constexpr double k_InvLn2 = 1.44269504088896338700e+00;
	constexpr double k_Q1 = -3.33333333333331316428e-02;
	constexpr double k_Q2 = 1.58730158725481460165e-03;
	constexpr double k_Q3 = -7.93650757867487942473e-05;
	constexpr double k_Q4 = 4.00821782732936239552e-06;
	constexpr double k_Q5 = -2.01099218183624371326e-07;

	double x = value;
	const uint32_t hx = HighWord(x) & 0x7FFF'FFFFu;
	const bool negative = (HighWord(x) >> 31) != 0;
	if (hx >= 0x4043'687Au)
	{
		// |x| of 56 ln2 or more
		if (IsNan(x))
		{
			return x;
		}
		if (negative)
		{
			return -1.0;
		}
		if (x > k_OverflowAbove)
		{
			return std::numeric_limits<double>::infinity();
		}
	}
	int k = 0;
	double c = 0.0;
	if (hx > 0x3FD6'2E42u)
	{
		// |x| more than 0.5 ln2
		double hi = 0.0;
		double lo = 0.0;
		if (hx < 0x3FF0'A2B2u)
		{
			// and less than 1.5 ln2
			if (!negative)
			{
				hi = x - k_Ln2Hi;
				lo = k_Ln2Lo;
				k = 1;
			}
			else
			{
				hi = x + k_Ln2Hi;
				lo = -k_Ln2Lo;
				k = -1;
			}
		}
		else
		{
			k = static_cast<int>(k_InvLn2 * x + (negative ? -0.5 : 0.5));
			const auto t = static_cast<double>(k);
			hi = x - t * k_Ln2Hi;
			lo = t * k_Ln2Lo;
		}
		x = hi - lo;
		c = (hi - x) - lo;
	}
	else if (hx < 0x3C90'0000u)
	{
		// |x| less than 2^-54
		return x;
	}
	const double hfx = 0.5 * x;
	const double hxs = x * hfx;
	const double r1 = 1.0 + hxs * (k_Q1 + hxs * (k_Q2 + hxs * (k_Q3 + hxs * (k_Q4 + hxs * k_Q5))));
	double t = 3.0 - r1 * hfx;
	double e = hxs * ((r1 - t) / (6.0 - x * t));
	if (k == 0)
	{
		return x - (x * e - hxs);
	}
	e = x * (e - c) - c;
	e -= hxs;
	if (k == -1)
	{
		return 0.5 * (x - e) - 0.5;
	}
	if (k == 1)
	{
		if (x < -0.25)
		{
			return -2.0 * (e - (x + 0.5));
		}
		return 1.0 + 2.0 * (x - e);
	}
	const double twopk = std::bit_cast<double>(static_cast<uint64_t>(0x3FF + k) << 52);
	if (k < 0 || k > 56)
	{
		double y = x - e + 1.0;
		if (k == 1024)
		{
			y = y * 2.0 * 0x1p1023;
		}
		else
		{
			y = y * twopk;
		}
		return y - 1.0;
	}
	const double twomk = std::bit_cast<double>(static_cast<uint64_t>(0x3FF - k) << 52);
	if (k < 20)
	{
		return (x - e + (1 - twomk)) * twopk;
	}
	return (x - (e + twomk) + 1) * twopk;
}

namespace
{

/// The steps of Log and Log2 for x out of the near-1 range: x = 2^k z, z in [OFF, 2 OFF); none for 0, a negative value,
/// infinity or not a number, which the caller answers
struct LogParts
{
	int k;
	double z;
	size_t i;
};

[[nodiscard]] std::optional<double> LogSpecial(double x, uint64_t& ix)
{
	const auto top = static_cast<uint32_t>(ix >> 48);
	if (top - 0x0010u >= 0x7FF0u - 0x0010u)
	{
		if (ix * 2 == 0)
		{
			return -std::numeric_limits<double>::infinity();
		}
		if (ix == std::bit_cast<uint64_t>(std::numeric_limits<double>::infinity()))
		{
			return x;
		}
		if ((top & 0x8000u) != 0 || (top & 0x7FF0u) == 0x7FF0u)
		{
			return std::numeric_limits<double>::quiet_NaN();
		}
		// A subnormal x, made normal
		ix = std::bit_cast<uint64_t>(x * 0x1p52);
		ix -= 52ull << 52;
	}
	return std::nullopt;
}

[[nodiscard]] LogParts SplitForLog(uint64_t ix, int tableBits)
{
	constexpr uint64_t k_Off = 0x3FE6'0000'0000'0000ull;
	const uint64_t tmp = ix - k_Off;
	const auto i = static_cast<size_t>((tmp >> (52 - tableBits)) % (uint64_t {1} << tableBits));
	const auto k = static_cast<int>(static_cast<int64_t>(tmp) >> 52);
	const uint64_t iz = ix - (tmp & (0xFFFull << 52));
	return {.k = k, .z = std::bit_cast<double>(iz), .i = i};
}

} // namespace

double Log(double value)
{
	namespace table = tables::log;
	const auto& b = table::k_Poly1;
	const auto& a = table::k_Poly;
	const double x = value;
	uint64_t ix = std::bit_cast<uint64_t>(x);
	constexpr uint64_t k_Low = std::bit_cast<uint64_t>(1.0 - 0x1p-4);
	constexpr uint64_t k_High = std::bit_cast<uint64_t>(1.0 + 0x1.09p-4);
	if (ix - k_Low < k_High - k_Low)
	{
		// Near 1
		if (ix == std::bit_cast<uint64_t>(1.0))
		{
			return 0.0;
		}
		const double r = x - 1.0;
		const double r2 = r * r;
		const double r3 = r * r2;
		double y = r3 * (b[1] + r * b[2] + r2 * b[3] +
		                 r3 * (b[4] + r * b[5] + r2 * b[6] + r3 * (b[7] + r * b[8] + r2 * b[9] + r3 * b[10])));
		double w = r * 0x1p27;
		const double rhi = r + w - w;
		const double rlo = r - rhi;
		w = rhi * rhi * b[0];
		const double hi = r + w;
		double lo = r - hi + w;
		lo += b[0] * rlo * (rhi + r);
		y += lo;
		y += hi;
		return y;
	}
	if (const auto special = LogSpecial(x, ix); special.has_value())
	{
		return *special;
	}
	const auto parts = SplitForLog(ix, table::k_TableBits);
	const double invc = table::k_Tab[parts.i].invc;
	const double logc = table::k_Tab[parts.i].logc;
	const double r = (parts.z - table::k_Tab2[parts.i].chi - table::k_Tab2[parts.i].clo) * invc;
	const auto kd = static_cast<double>(parts.k);
	const double w = kd * table::k_Ln2Hi + logc;
	const double hi = w + r;
	const double lo = w - hi + r + kd * table::k_Ln2Lo;
	const double r2 = r * r;
	return lo + r2 * a[0] + r * r2 * (a[1] + r * a[2] + r2 * (a[3] + r * a[4])) + hi;
}

double Log2(double value)
{
	namespace table = tables::log2;
	const auto& b = table::k_Poly1;
	const auto& a = table::k_Poly;
	const double x = value;
	uint64_t ix = std::bit_cast<uint64_t>(x);
	constexpr uint64_t k_Low = std::bit_cast<uint64_t>(1.0 - 0x1.5b51p-5);
	constexpr uint64_t k_High = std::bit_cast<uint64_t>(1.0 + 0x1.6ab2p-5);
	const auto highHalf = [](double v) { return std::bit_cast<double>(std::bit_cast<uint64_t>(v) & (~0ull << 32)); };
	if (ix - k_Low < k_High - k_Low)
	{
		// Near 1
		if (ix == std::bit_cast<uint64_t>(1.0))
		{
			return 0.0;
		}
		const double r = x - 1.0;
		const double rhi = highHalf(r);
		const double rlo = r - rhi;
		const double hi = rhi * table::k_InvLn2Hi;
		double lo = rlo * table::k_InvLn2Hi + r * table::k_InvLn2Lo;
		const double r2 = r * r;
		const double r4 = r2 * r2;
		const double p = r2 * (b[0] + r * b[1]);
		double y = hi + p;
		lo += hi - y + p;
		lo += r4 * (b[2] + r * b[3] + r2 * (b[4] + r * b[5]) + r4 * (b[6] + r * b[7] + r2 * (b[8] + r * b[9])));
		y += lo;
		return y;
	}
	if (const auto special = LogSpecial(x, ix); special.has_value())
	{
		return *special;
	}
	const auto parts = SplitForLog(ix, table::k_TableBits);
	const double invc = table::k_Tab[parts.i].invc;
	const double logc = table::k_Tab[parts.i].logc;
	const auto kd = static_cast<double>(parts.k);
	const double r = (parts.z - table::k_Tab2[parts.i].chi - table::k_Tab2[parts.i].clo) * invc;
	const double rhi = highHalf(r);
	const double rlo = r - rhi;
	const double t1 = rhi * table::k_InvLn2Hi;
	const double t2 = rlo * table::k_InvLn2Hi + r * table::k_InvLn2Lo;
	const double t3 = kd + logc;
	const double hi = t3 + t1;
	const double lo = t3 - hi + t1 + t2;
	const double r2 = r * r;
	const double r4 = r2 * r2;
	const double p = a[0] + r * a[1] + r2 * (a[2] + r * a[3]) + r4 * (a[4] + r * a[5]);
	return lo + r2 * p + hi;
}

float Sin(float radians)
{
	return static_cast<float>(Sin(static_cast<double>(radians)));
}

float Cos(float radians)
{
	return static_cast<float>(Cos(static_cast<double>(radians)));
}

float Tan(float radians)
{
	return static_cast<float>(Tan(static_cast<double>(radians)));
}

float Atan(float value)
{
	return static_cast<float>(Atan(static_cast<double>(value)));
}

float Atan2(float y, float x)
{
	return static_cast<float>(Atan2(static_cast<double>(y), static_cast<double>(x)));
}

float Acos(float value)
{
	return static_cast<float>(Acos(static_cast<double>(value)));
}

float Exp(float value)
{
	return static_cast<float>(Exp(static_cast<double>(value)));
}

float Log(float value)
{
	return static_cast<float>(Log(static_cast<double>(value)));
}

float Log2(float value)
{
	return static_cast<float>(Log2(static_cast<double>(value)));
}

} // namespace openblack::portable_math
