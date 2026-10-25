/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>
#include <limits>
#include <string_view>

#include <glm/geometric.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "Common/PortableMath.h"
#include "Common/PortableRotations.h"

using namespace openblack;

namespace
{

/// A result and the exact value rounded to a double (from a 200-bit reference), and the same for the float function
struct Case
{
	std::string_view name;
	double (*wide)();
	double wideExpected;
	float (*narrow)();
	double narrowExpected;
};

struct DoubleCase
{
	std::string_view name;
	double (*wide)();
	double expected;
};

/// The distance of two doubles in units in the last place
[[nodiscard]] uint64_t UlpDistance(double a, double b)
{
	const auto ordered = [](double value) {
		const auto bits = std::bit_cast<int64_t>(value);
		return bits < 0 ? std::numeric_limits<int64_t>::min() - bits : bits;
	};
	const auto x = ordered(a);
	const auto y = ordered(b);
	return static_cast<uint64_t>(x > y ? x - y : y - x);
}

// Made by a script from mpmath at 200 bits
const std::array k_Cases = {
    Case {"Sin", [] { return portable_math::Sin(0x1.99999a0000000p-4); }, 0x1.98eaed1ea022bp-4,
          [] { return portable_math::Sin(static_cast<float>(0x1.99999a0000000p-4)); }, 0x1.98eaee0000000p-4},
    Case {"Cos", [] { return portable_math::Cos(0x1.99999a0000000p-4); }, 0x1.fd712f993a59bp-1,
          [] { return portable_math::Cos(static_cast<float>(0x1.99999a0000000p-4)); }, 0x1.fd71300000000p-1},
    Case {"Tan", [] { return portable_math::Tan(0x1.99999a0000000p-4); }, 0x1.9af887db9f053p-4,
          [] { return portable_math::Tan(static_cast<float>(0x1.99999a0000000p-4)); }, 0x1.9af8880000000p-4},
    Case {"Sin", [] { return portable_math::Sin(0x1.0000000000000p-1); }, 0x1.eaee8744b05f0p-2,
          [] { return portable_math::Sin(static_cast<float>(0x1.0000000000000p-1)); }, 0x1.eaee880000000p-2},
    Case {"Cos", [] { return portable_math::Cos(0x1.0000000000000p-1); }, 0x1.c1528065b7d50p-1,
          [] { return portable_math::Cos(static_cast<float>(0x1.0000000000000p-1)); }, 0x1.c152800000000p-1},
    Case {"Tan", [] { return portable_math::Tan(0x1.0000000000000p-1); }, 0x1.17b4f5bf3474ap-1,
          [] { return portable_math::Tan(static_cast<float>(0x1.0000000000000p-1)); }, 0x1.17b4f60000000p-1},
    Case {"Sin", [] { return portable_math::Sin(0x1.0000000000000p+0); }, 0x1.aed548f090ceep-1,
          [] { return portable_math::Sin(static_cast<float>(0x1.0000000000000p+0)); }, 0x1.aed5480000000p-1},
    Case {"Cos", [] { return portable_math::Cos(0x1.0000000000000p+0); }, 0x1.14a280fb5068cp-1,
          [] { return portable_math::Cos(static_cast<float>(0x1.0000000000000p+0)); }, 0x1.14a2800000000p-1},
    Case {"Tan", [] { return portable_math::Tan(0x1.0000000000000p+0); }, 0x1.8eb245cbee3a6p+0,
          [] { return portable_math::Tan(static_cast<float>(0x1.0000000000000p+0)); }, 0x1.8eb2460000000p+0},
    Case {"Sin", [] { return portable_math::Sin(0x1.921fb60000000p+0); }, 0x1.ffffffffffff7p-1,
          [] { return portable_math::Sin(static_cast<float>(0x1.921fb60000000p+0)); }, 0x1.0000000000000p+0},
    Case {"Cos", [] { return portable_math::Cos(0x1.921fb60000000p+0); }, -0x1.777a5cf72ceccp-25,
          [] { return portable_math::Cos(static_cast<float>(0x1.921fb60000000p+0)); }, -0x1.777a5c0000000p-25},
    Case {"Tan", [] { return portable_math::Tan(0x1.921fb60000000p+0); }, -0x1.5d14946dc9897p+24,
          [] { return portable_math::Tan(static_cast<float>(0x1.921fb60000000p+0)); }, -0x1.5d14940000000p+24},
    Case {"Sin", [] { return portable_math::Sin(0x1.0000000000000p+1); }, 0x1.d18f6ead1b446p-1,
          [] { return portable_math::Sin(static_cast<float>(0x1.0000000000000p+1)); }, 0x1.d18f6e0000000p-1},
    Case {"Cos", [] { return portable_math::Cos(0x1.0000000000000p+1); }, -0x1.aa22657537205p-2,
          [] { return portable_math::Cos(static_cast<float>(0x1.0000000000000p+1)); }, -0x1.aa22660000000p-2},
    Case {"Tan", [] { return portable_math::Tan(0x1.0000000000000p+1); }, -0x1.17af62e0950f8p+1,
          [] { return portable_math::Tan(static_cast<float>(0x1.0000000000000p+1)); }, -0x1.17af620000000p+1},
    Case {"Sin", [] { return portable_math::Sin(0x1.921fb60000000p+1); }, -0x1.777a5cf72cec6p-24,
          [] { return portable_math::Sin(static_cast<float>(0x1.921fb60000000p+1)); }, -0x1.777a5c0000000p-24},
    Case {"Cos", [] { return portable_math::Cos(0x1.921fb60000000p+1); }, -0x1.fffffffffffdep-1,
          [] { return portable_math::Cos(static_cast<float>(0x1.921fb60000000p+1)); }, -0x1.0000000000000p+0},
    Case {"Tan", [] { return portable_math::Tan(0x1.921fb60000000p+1); }, 0x1.777a5cf72cedfp-24,
          [] { return portable_math::Tan(static_cast<float>(0x1.921fb60000000p+1)); }, 0x1.777a5c0000000p-24},
    Case {"Sin", [] { return portable_math::Sin(0x1.0000000000000p+2); }, -0x1.837b9dddc1eaep-1,
          [] { return portable_math::Sin(static_cast<float>(0x1.0000000000000p+2)); }, -0x1.837b9e0000000p-1},
    Case {"Cos", [] { return portable_math::Cos(0x1.0000000000000p+2); }, -0x1.4eaa606db24c1p-1,
          [] { return portable_math::Cos(static_cast<float>(0x1.0000000000000p+2)); }, -0x1.4eaa600000000p-1},
    Case {"Tan", [] { return portable_math::Tan(0x1.0000000000000p+2); }, 0x1.2866f9be4de13p+0,
          [] { return portable_math::Tan(static_cast<float>(0x1.0000000000000p+2)); }, 0x1.2866fa0000000p+0},
    Case {"Sin", [] { return portable_math::Sin(0x1.921fb60000000p+2); }, 0x1.777a5cf72ceadp-23,
          [] { return portable_math::Sin(static_cast<float>(0x1.921fb60000000p+2)); }, 0x1.777a5c0000000p-23},
    Case {"Cos", [] { return portable_math::Cos(0x1.921fb60000000p+2); }, 0x1.fffffffffff76p-1,
          [] { return portable_math::Cos(static_cast<float>(0x1.921fb60000000p+2)); }, 0x1.0000000000000p+0},
    Case {"Tan", [] { return portable_math::Tan(0x1.921fb60000000p+2); }, 0x1.777a5cf72cf12p-23,
          [] { return portable_math::Tan(static_cast<float>(0x1.921fb60000000p+2)); }, 0x1.777a5c0000000p-23},
    Case {"Sin", [] { return portable_math::Sin(0x1.4000000000000p+3); }, -0x1.1689ef5f34f52p-1,
          [] { return portable_math::Sin(static_cast<float>(0x1.4000000000000p+3)); }, -0x1.1689f00000000p-1},
    Case {"Cos", [] { return portable_math::Cos(0x1.4000000000000p+3); }, -0x1.ad9ac890c6b1fp-1,
          [] { return portable_math::Cos(static_cast<float>(0x1.4000000000000p+3)); }, -0x1.ad9ac80000000p-1},
    Case {"Tan", [] { return portable_math::Tan(0x1.4000000000000p+3); }, 0x1.4bf5f34be3782p-1,
          [] { return portable_math::Tan(static_cast<float>(0x1.4000000000000p+3)); }, 0x1.4bf5f40000000p-1},
    Case {"Sin", [] { return portable_math::Sin(0x1.9000000000000p+6); }, -0x1.03425b78c4db8p-1,
          [] { return portable_math::Sin(static_cast<float>(0x1.9000000000000p+6)); }, -0x1.03425c0000000p-1},
    Case {"Cos", [] { return portable_math::Cos(0x1.9000000000000p+6); }, 0x1.b981dbf665fdfp-1,
          [] { return portable_math::Cos(static_cast<float>(0x1.9000000000000p+6)); }, 0x1.b981dc0000000p-1},
    Case {"Tan", [] { return portable_math::Tan(0x1.9000000000000p+6); }, -0x1.2ca74d62b5d38p-1,
          [] { return portable_math::Tan(static_cast<float>(0x1.9000000000000p+6)); }, -0x1.2ca74e0000000p-1},
    Case {"Sin", [] { return portable_math::Sin(0x1.f400000000000p+9); }, 0x1.a75cc150a206bp-1,
          [] { return portable_math::Sin(static_cast<float>(0x1.f400000000000p+9)); }, 0x1.a75cc20000000p-1},
    Case {"Cos", [] { return portable_math::Cos(0x1.f400000000000p+9); }, 0x1.1ff026793f1bbp-1,
          [] { return portable_math::Cos(static_cast<float>(0x1.f400000000000p+9)); }, 0x1.1ff0260000000p-1},
    Case {"Tan", [] { return portable_math::Tan(0x1.f400000000000p+9); }, 0x1.786729f34311ap+0,
          [] { return portable_math::Tan(static_cast<float>(0x1.f400000000000p+9)); }, 0x1.78672a0000000p+0},
    Case {"Sin", [] { return portable_math::Sin(-0x1.8000000000000p-1); }, -0x1.5cffc16bf8f0dp-1,
          [] { return portable_math::Sin(static_cast<float>(-0x1.8000000000000p-1)); }, -0x1.5cffc20000000p-1},
    Case {"Cos", [] { return portable_math::Cos(-0x1.8000000000000p-1); }, 0x1.769fec655211fp-1,
          [] { return portable_math::Cos(static_cast<float>(-0x1.8000000000000p-1)); }, 0x1.769fec0000000p-1},
    Case {"Tan", [] { return portable_math::Tan(-0x1.8000000000000p-1); }, -0x1.dcfa36110eeecp-1,
          [] { return portable_math::Tan(static_cast<float>(-0x1.8000000000000p-1)); }, -0x1.dcfa360000000p-1},
    Case {"Sin", [] { return portable_math::Sin(-0x1.8000000000000p+1); }, -0x1.210386db6d55bp-3,
          [] { return portable_math::Sin(static_cast<float>(-0x1.8000000000000p+1)); }, -0x1.2103860000000p-3},
    Case {"Cos", [] { return portable_math::Cos(-0x1.8000000000000p+1); }, -0x1.fae04be85e5d2p-1,
          [] { return portable_math::Cos(static_cast<float>(-0x1.8000000000000p+1)); }, -0x1.fae04c0000000p-1},
    Case {"Tan", [] { return portable_math::Tan(-0x1.8000000000000p+1); }, 0x1.23ef71254b86fp-3,
          [] { return portable_math::Tan(static_cast<float>(-0x1.8000000000000p+1)); }, 0x1.23ef720000000p-3},
    Case {"Sin", [] { return portable_math::Sin(0x1.921fb60000000p-1); }, 0x1.6a09e6ecb41fep-1,
          [] { return portable_math::Sin(static_cast<float>(0x1.921fb60000000p-1)); }, 0x1.6a09e60000000p-1},
    Case {"Cos", [] { return portable_math::Cos(0x1.921fb60000000p-1); }, 0x1.6a09e5e333598p-1,
          [] { return portable_math::Cos(static_cast<float>(0x1.921fb60000000p-1)); }, 0x1.6a09e60000000p-1},
    Case {"Tan", [] { return portable_math::Tan(0x1.921fb60000000p-1); }, 0x1.000000bbbd2ecp+0,
          [] { return portable_math::Tan(static_cast<float>(0x1.921fb60000000p-1)); }, 0x1.0000000000000p+0},
    Case {"Sin", [] { return portable_math::Sin(0x1.4f8b580000000p-17); }, 0x1.4f8b57ffe7fb1p-17,
          [] { return portable_math::Sin(static_cast<float>(0x1.4f8b580000000p-17)); }, 0x1.4f8b580000000p-17},
    Case {"Cos", [] { return portable_math::Cos(0x1.4f8b580000000p-17); }, 0x1.ffffffff920c8p-1,
          [] { return portable_math::Cos(static_cast<float>(0x1.4f8b580000000p-17)); }, 0x1.0000000000000p+0},
    Case {"Tan", [] { return portable_math::Tan(0x1.4f8b580000000p-17); }, 0x1.4f8b58003009dp-17,
          [] { return portable_math::Tan(static_cast<float>(0x1.4f8b580000000p-17)); }, 0x1.4f8b580000000p-17},
    Case {"Atan", [] { return portable_math::Atan(0x1.47ae140000000p-7); }, 0x1.47ab483711936p-7,
          [] { return portable_math::Atan(static_cast<float>(0x1.47ae140000000p-7)); }, 0x1.47ab480000000p-7},
    Case {"Atan", [] { return portable_math::Atan(0x1.3333340000000p-2); }, 0x1.2a73a71dcec16p-2,
          [] { return portable_math::Atan(static_cast<float>(0x1.3333340000000p-2)); }, 0x1.2a73a80000000p-2},
    Case {"Atan", [] { return portable_math::Atan(0x1.cccccc0000000p-1); }, 0x1.77338a0f3a0bcp-1,
          [] { return portable_math::Atan(static_cast<float>(0x1.cccccc0000000p-1)); }, 0x1.77338a0000000p-1},
    Case {"Atan", [] { return portable_math::Atan(0x1.0000000000000p+0); }, 0x1.921fb54442d18p-1,
          [] { return portable_math::Atan(static_cast<float>(0x1.0000000000000p+0)); }, 0x1.921fb60000000p-1},
    Case {"Atan", [] { return portable_math::Atan(0x1.b333340000000p+0); }, 0x1.0a00a3f189411p+0,
          [] { return portable_math::Atan(static_cast<float>(0x1.b333340000000p+0)); }, 0x1.0a00a40000000p+0},
    Case {"Atan", [] { return portable_math::Atan(0x1.4000000000000p+2); }, 0x1.5f97315254857p+0,
          [] { return portable_math::Atan(static_cast<float>(0x1.4000000000000p+2)); }, 0x1.5f97320000000p+0},
    Case {"Atan", [] { return portable_math::Atan(0x1.2c00000000000p+8); }, 0x1.9145416ba1b13p+0,
          [] { return portable_math::Atan(static_cast<float>(0x1.2c00000000000p+8)); }, 0x1.9145420000000p+0},
    Case {"Atan", [] { return portable_math::Atan(-0x1.4000000000000p+1); }, -0x1.30b6d796a4da8p+0,
          [] { return portable_math::Atan(static_cast<float>(-0x1.4000000000000p+1)); }, -0x1.30b6d80000000p+0},
    Case {
        "Atan2", [] { return portable_math::Atan2(0x1.0000000000000p+0, 0x1.0000000000000p+0); }, 0x1.921fb54442d18p-1,
        [] { return portable_math::Atan2(static_cast<float>(0x1.0000000000000p+0), static_cast<float>(0x1.0000000000000p+0)); },
        0x1.921fb60000000p-1},
    Case {"Atan2", [] { return portable_math::Atan2(0x1.0000000000000p-1, -0x1.0000000000000p+1); }, 0x1.72c43f4b1650ap+1,
          [] {
	          return portable_math::Atan2(static_cast<float>(0x1.0000000000000p-1), static_cast<float>(-0x1.0000000000000p+1));
          },
          0x1.72c4400000000p+1},
    Case {"Atan2", [] { return portable_math::Atan2(-0x1.8000000000000p+1, -0x1.0000000000000p-2); }, -0x1.a76873ac2bc7cp+0,
          [] {
	          return portable_math::Atan2(static_cast<float>(-0x1.8000000000000p+1), static_cast<float>(-0x1.0000000000000p-2));
          },
          -0x1.a768740000000p+0},
    Case {"Atan2", [] { return portable_math::Atan2(-0x1.0624de0000000p-10, 0x1.0000000000000p+2); }, -0x1.0624dda45fbfep-12,
          [] {
	          return portable_math::Atan2(static_cast<float>(-0x1.0624de0000000p-10), static_cast<float>(0x1.0000000000000p+2));
          },
          -0x1.0624de0000000p-12},
    Case {"Atan2", [] { return portable_math::Atan2(0x1.c000000000000p+2, 0x1.0624de0000000p-10); }, 0x1.921658857abe8p+0,
          [] {
	          return portable_math::Atan2(static_cast<float>(0x1.c000000000000p+2), static_cast<float>(0x1.0624de0000000p-10));
          },
          0x1.9216580000000p+0},
    Case {"Atan2", [] { return portable_math::Atan2(0x1.0000000000000p-2, -0x1.6666660000000p-1); }, 0x1.66377fd9c0560p+1,
          [] {
	          return portable_math::Atan2(static_cast<float>(0x1.0000000000000p-2), static_cast<float>(-0x1.6666660000000p-1));
          },
          0x1.6637800000000p+1},
    Case {"Acos", [] { return portable_math::Acos(-0x1.fae1480000000p-1); }, 0x1.8001beacedde7p+1,
          [] { return portable_math::Acos(static_cast<float>(-0x1.fae1480000000p-1)); }, 0x1.8001be0000000p+1},
    Case {"Acos", [] { return portable_math::Acos(-0x1.3333340000000p-1); }, 0x1.1b6e196ebbe45p+1,
          [] { return portable_math::Acos(static_cast<float>(-0x1.3333340000000p-1)); }, 0x1.1b6e1a0000000p+1},
    Case {"Acos", [] { return portable_math::Acos(-0x1.99999a0000000p-3); }, 0x1.c5abe6a5e8f4bp+0,
          [] { return portable_math::Acos(static_cast<float>(-0x1.99999a0000000p-3)); }, 0x1.c5abe60000000p+0},
    Case {"Acos", [] { return portable_math::Acos(0x0.0p+0); }, 0x1.921fb54442d18p+0,
          [] { return portable_math::Acos(static_cast<float>(0x0.0p+0)); }, 0x1.921fb60000000p+0},
    Case {"Acos", [] { return portable_math::Acos(0x1.3333340000000p-2); }, 0x1.441f5e9642e15p+0,
          [] { return portable_math::Acos(static_cast<float>(0x1.3333340000000p-2)); }, 0x1.441f5e0000000p+0},
    Case {"Acos", [] { return portable_math::Acos(0x1.6666660000000p-1); }, 0x1.973e8485394a3p-1,
          [] { return portable_math::Acos(static_cast<float>(0x1.6666660000000p-1)); }, 0x1.973e840000000p-1},
    Case {"Acos", [] { return portable_math::Acos(0x1.ff7cee0000000p-1); }, 0x1.6e62b3bdcb95ep-5,
          [] { return portable_math::Acos(static_cast<float>(0x1.ff7cee0000000p-1)); }, 0x1.6e62b40000000p-5},
    Case {"Exp", [] { return portable_math::Exp(-0x1.4000000000000p+3); }, 0x1.7cd79b5647c9bp-15,
          [] { return portable_math::Exp(static_cast<float>(-0x1.4000000000000p+3)); }, 0x1.7cd79c0000000p-15},
    Case {"Exp", [] { return portable_math::Exp(-0x1.0000000000000p+0); }, 0x1.78b56362cef38p-2,
          [] { return portable_math::Exp(static_cast<float>(-0x1.0000000000000p+0)); }, 0x1.78b5640000000p-2},
    Case {"Exp", [] { return portable_math::Exp(-0x1.99999a0000000p-4); }, 0x1.cf46d991bdbb0p-1,
          [] { return portable_math::Exp(static_cast<float>(-0x1.99999a0000000p-4)); }, 0x1.cf46da0000000p-1},
    Case {"Exp", [] { return portable_math::Exp(0x1.0000000000000p-1); }, 0x1.a61298e1e069cp+0,
          [] { return portable_math::Exp(static_cast<float>(0x1.0000000000000p-1)); }, 0x1.a612980000000p+0},
    Case {"Exp", [] { return portable_math::Exp(0x1.0000000000000p+0); }, 0x1.5bf0a8b145769p+1,
          [] { return portable_math::Exp(static_cast<float>(0x1.0000000000000p+0)); }, 0x1.5bf0a80000000p+1},
    Case {"Exp", [] { return portable_math::Exp(0x1.4000000000000p+1); }, 0x1.85d6fd931e0bbp+3,
          [] { return portable_math::Exp(static_cast<float>(0x1.4000000000000p+1)); }, 0x1.85d6fe0000000p+3},
    Case {"Exp", [] { return portable_math::Exp(0x1.4000000000000p+3); }, 0x1.5829dcf950560p+14,
          [] { return portable_math::Exp(static_cast<float>(0x1.4000000000000p+3)); }, 0x1.5829dc0000000p+14},
    Case {"Exp", [] { return portable_math::Exp(0x1.9000000000000p+5); }, 0x1.19103e4080b45p+72,
          [] { return portable_math::Exp(static_cast<float>(0x1.9000000000000p+5)); }, 0x1.19103e0000000p+72},
    Case {"Log", [] { return portable_math::Log(0x1.0624de0000000p-10); }, -0x1.ba18a965fffa2p+2,
          [] { return portable_math::Log(static_cast<float>(0x1.0624de0000000p-10)); }, -0x1.ba18aa0000000p+2},
    Case {"Log2", [] { return portable_math::Log2(0x1.0624de0000000p-10); }, -0x1.3ee7b44ce9bf9p+3,
          [] { return portable_math::Log2(static_cast<float>(0x1.0624de0000000p-10)); }, -0x1.3ee7b40000000p+3},
    Case {"Log", [] { return portable_math::Log(0x1.0000000000000p-1); }, -0x1.62e42fefa39efp-1,
          [] { return portable_math::Log(static_cast<float>(0x1.0000000000000p-1)); }, -0x1.62e4300000000p-1},
    Case {"Log2", [] { return portable_math::Log2(0x1.0000000000000p-1); }, -0x1.0000000000000p+0,
          [] { return portable_math::Log2(static_cast<float>(0x1.0000000000000p-1)); }, -0x1.0000000000000p+0},
    Case {"Log", [] { return portable_math::Log(0x1.fae1480000000p-1); }, -0x1.49543f3726accp-7,
          [] { return portable_math::Log(static_cast<float>(0x1.fae1480000000p-1)); }, -0x1.4954400000000p-7},
    Case {"Log2", [] { return portable_math::Log2(0x1.fae1480000000p-1); }, -0x1.db1f16fa7be98p-7,
          [] { return portable_math::Log2(static_cast<float>(0x1.fae1480000000p-1)); }, -0x1.db1f160000000p-7},
    Case {"Log", [] { return portable_math::Log(0x1.028f5c0000000p+0); }, 0x1.460d5885ab007p-7,
          [] { return portable_math::Log(static_cast<float>(0x1.028f5c0000000p+0)); }, 0x1.460d580000000p-7},
    Case {"Log2", [] { return portable_math::Log2(0x1.028f5c0000000p+0); }, 0x1.d664cfad39024p-7,
          [] { return portable_math::Log2(static_cast<float>(0x1.028f5c0000000p+0)); }, 0x1.d664d00000000p-7},
    Case {"Log", [] { return portable_math::Log(0x1.0000000000000p+1); }, 0x1.62e42fefa39efp-1,
          [] { return portable_math::Log(static_cast<float>(0x1.0000000000000p+1)); }, 0x1.62e4300000000p-1},
    Case {"Log2", [] { return portable_math::Log2(0x1.0000000000000p+1); }, 0x1.0000000000000p+0,
          [] { return portable_math::Log2(static_cast<float>(0x1.0000000000000p+1)); }, 0x1.0000000000000p+0},
    Case {"Log", [] { return portable_math::Log(0x1.4000000000000p+3); }, 0x1.26bb1bbb55516p+1,
          [] { return portable_math::Log(static_cast<float>(0x1.4000000000000p+3)); }, 0x1.26bb1c0000000p+1},
    Case {"Log2", [] { return portable_math::Log2(0x1.4000000000000p+3); }, 0x1.a934f0979a371p+1,
          [] { return portable_math::Log2(static_cast<float>(0x1.4000000000000p+3)); }, 0x1.a934f00000000p+1},
    Case {"Log", [] { return portable_math::Log(0x1.f400000000000p+9); }, 0x1.ba18a998fffa0p+2,
          [] { return portable_math::Log(static_cast<float>(0x1.f400000000000p+9)); }, 0x1.ba18aa0000000p+2},
    Case {"Log2", [] { return portable_math::Log2(0x1.f400000000000p+9); }, 0x1.3ee7b471b3a95p+3,
          [] { return portable_math::Log2(static_cast<float>(0x1.f400000000000p+9)); }, 0x1.3ee7b40000000p+3},
    Case {"Log", [] { return portable_math::Log(0x1.81cd6c0000000p+13); }, 0x1.2d79558c04cd4p+3,
          [] { return portable_math::Log(static_cast<float>(0x1.81cd6c0000000p+13)); }, 0x1.2d79560000000p+3},
    Case {"Log2", [] { return portable_math::Log2(0x1.81cd6c0000000p+13); }, 0x1.b2ef5b763c10fp+3,
          [] { return portable_math::Log2(static_cast<float>(0x1.81cd6c0000000p+13)); }, 0x1.b2ef5c0000000p+3},
};

const std::array k_DoubleCases = {
    DoubleCase {"Expm1", [] { return portable_math::Expm1(-0x1.0000000000000p+0); }, -0x1.43a54e4e98864p-1},
    DoubleCase {"Expm1", [] { return portable_math::Expm1(-0x1.0624de0000000p-10); }, -0x1.060352ed5c35ap-10},
    DoubleCase {"Expm1", [] { return portable_math::Expm1(0x1.0c6f7a0000000p-20); }, 0x1.0c6f82cbccf11p-20},
    DoubleCase {"Expm1", [] { return portable_math::Expm1(0x1.3333340000000p-2); }, 0x1.664164377a1fcp-2},
    DoubleCase {"Expm1", [] { return portable_math::Expm1(0x1.0000000000000p+1); }, 0x1.98e64b8d4ddaep+2},
};

} // namespace

TEST(PortableMath, DoubleResultsAreWithinOneUnitOfTheExactValue)
{
	for (const auto& test : k_Cases)
	{
		EXPECT_LE(UlpDistance(test.wide(), test.wideExpected), 1u) << test.name;
	}
	for (const auto& test : k_DoubleCases)
	{
		EXPECT_LE(UlpDistance(test.wide(), test.expected), 1u) << test.name;
	}
}

TEST(PortableMath, FloatResultsAreTheExactValueRoundedOnce)
{
	for (const auto& test : k_Cases)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(test.narrow()), std::bit_cast<uint32_t>(static_cast<float>(test.narrowExpected)))
		    << test.name;
	}
}

TEST(PortableMath, FloatResultsAreTheDoubleResultRounded)
{
	for (float x = -20.0f; x < 20.0f; x += 0.0137f)
	{
		EXPECT_EQ(portable_math::Sin(x), static_cast<float>(portable_math::Sin(static_cast<double>(x))));
		EXPECT_EQ(portable_math::Cos(x), static_cast<float>(portable_math::Cos(static_cast<double>(x))));
		EXPECT_EQ(portable_math::Atan2(x, 0.3f),
		          static_cast<float>(portable_math::Atan2(static_cast<double>(x), static_cast<double>(0.3f))));
	}
}

TEST(PortableMath, EdgeValues)
{
	EXPECT_EQ(portable_math::Sin(0.0), 0.0);
	EXPECT_EQ(portable_math::Cos(0.0), 1.0);
	EXPECT_TRUE(std::isnan(portable_math::Sin(std::numeric_limits<double>::infinity())));
	EXPECT_TRUE(std::isnan(portable_math::Acos(1.5)));
	EXPECT_EQ(portable_math::Acos(1.0), 0.0);
	EXPECT_EQ(portable_math::Log(1.0), 0.0);
	EXPECT_EQ(portable_math::Log2(8.0), 3.0);
	EXPECT_EQ(portable_math::Log(0.0), -std::numeric_limits<double>::infinity());
	EXPECT_TRUE(std::isnan(portable_math::Log(-1.0)));
	EXPECT_EQ(portable_math::Exp(0.0), 1.0);
	EXPECT_EQ(portable_math::Exp(-1000.0), 0.0);
	EXPECT_EQ(portable_math::Exp(1000.0), std::numeric_limits<double>::infinity());
	EXPECT_EQ(portable_math::Atan2(0.0, -1.0), std::bit_cast<double>(0x400921FB54442D18ull));
	EXPECT_EQ(portable_math::Expm1(0.0), 0.0);
}

TEST(PortableRotations, MatchGlmWhereTheSineIsTheSame)
{
	// Angles where the C library's float sine and cosine are the exact values rounded once, as portable_math gives
	for (const float angle : {0.0f, 0.5f, -1.25f, 2.0f})
	{
		const float c = portable_math::Cos(angle);
		const float s = portable_math::Sin(angle);
		const auto y = portable_math::EulerAngleY(angle);
		EXPECT_EQ(y[0][0], c);
		EXPECT_EQ(y[0][2], -s);
		EXPECT_EQ(y[2][0], s);
		EXPECT_EQ(y[2][2], c);
		const auto turned = portable_math::Rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f));
		EXPECT_EQ(turned[0][0], c + (1.0f - c) * 0.0f);
		EXPECT_EQ(turned[1][1], c + (1.0f - c) * 1.0f);
	}
}

TEST(GlmDot, AddsTheFourPartsLeftToRightOnEveryCompiler)
{
	// Values of a tree's bounding centre where the other order of the additions gives another float
	const glm::vec4 row(std::bit_cast<float>(0xbb1afef8u), 0.0f, std::bit_cast<float>(0x3f7d7075u),
	                    std::bit_cast<float>(0x44db62e1u));
	const glm::vec4 point(std::bit_cast<float>(0xbd816600u), std::bit_cast<float>(0x40500ea5u),
	                      std::bit_cast<float>(0x3dfe6200u), 1.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(glm::dot(row, point)), 0x44db66d2u);
}
