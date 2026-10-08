/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Noise.h"

#include <cmath>
#include <cstdint>

#include <array>

#include "Common/GameRandom.h"

using namespace openblack::psys;

namespace
{
/// The permutation table (Ebert's `perm`)
constexpr std::array<uint8_t, 256> k_Permutation {
    225, 155, 210, 108, 175, 199, 221, 144, 203, 116, 70,  213, 69,  158, 33,  252, 5,   82,  173, 133, 222, 139, 174, 27,
    9,   71,  90,  246, 75,  130, 91,  191, 169, 138, 2,   151, 194, 235, 81,  7,   25,  113, 228, 159, 205, 253, 134, 142,
    248, 65,  224, 217, 22,  121, 229, 63,  89,  103, 96,  104, 156, 17,  201, 129, 36,  8,   165, 110, 237, 117, 231, 56,
    132, 211, 152, 20,  181, 111, 239, 218, 170, 163, 51,  172, 157, 47,  80,  212, 176, 250, 87,  49,  99,  242, 136, 189,
    162, 115, 44,  43,  124, 94,  150, 16,  141, 247, 32,  10,  198, 223, 255, 72,  53,  131, 84,  57,  220, 197, 58,  50,
    208, 11,  241, 28,  3,   192, 62,  202, 18,  215, 153, 24,  76,  41,  15,  179, 39,  46,  55,  6,   128, 167, 23,  188,
    106, 34,  187, 140, 164, 73,  112, 182, 244, 195, 227, 13,  35,  77,  196, 185, 26,  200, 226, 119, 31,  123, 168, 125,
    249, 68,  183, 230, 177, 135, 160, 180, 12,  1,   243, 148, 102, 166, 38,  238, 251, 37,  240, 126, 64,  74,  161, 40,
    184, 149, 171, 178, 101, 66,  29,  59,  146, 61,  254, 107, 42,  86,  154, 4,   236, 232, 120, 21,  233, 209, 45,  98,
    193, 114, 78,  19,  206, 14,  118, 127, 48,  79,  147, 85,  30,  207, 219, 54,  88,  234, 190, 122, 95,  67,  143, 109,
    137, 214, 145, 93,  92,  100, 245, 0,   216, 186, 60,  83,  105, 97,  204, 52};

/// The lattice values: filled once, at the game's one-time start-up, with 1 - the game's float rand(2). That runs
/// before the game sets its seeds, on the seed the game was made with: 0 (inferred: the game object is zeroed when it
/// is made). The same values every game, and the game's own seed is not moved by these 256 draws
const std::array<float, 256>& Values()
{
	static const auto values = [] {
		std::array<float, 256> v {};
		uint32_t seed = 0;
		for (auto& value : v)
		{
			const float r = openblack::game_random::FloatRand(2.0f, seed);
			value = 1.0f - r;
		}
		return v;
	}();
	return values;
}
} // namespace

float noise::Spline(float x, int count, const float* knots)
{
	const int spans = count - 3;
	x = x <= 0.0f ? 0.0f : (x < 1.0f ? x : 1.0f);
	x *= static_cast<float>(spans);
	int span = static_cast<int>(x);
	if (span >= spans)
	{
		span = spans - 1;
	}
	x -= static_cast<float>(span);
	const float* k = knots + span;
	// the Catmull-Rom basis
	const float c3 = -0.5f * k[0] + 1.5f * k[1] - 1.5f * k[2] + 0.5f * k[3];
	const float c2 = k[0] - 2.5f * k[1] + 2.0f * k[2] - 0.5f * k[3];
	const float c1 = -0.5f * k[0] + 0.5f * k[2];
	const float c0 = k[1];
	return ((c3 * x + c2) * x + c1) * x + c0;
}

float noise::Lattice(int i)
{
	return Values()[k_Permutation[static_cast<size_t>(i & 0xFF)]];
}

float noise::SignedValueNoise(float x)
{
	const int ix = static_cast<int>(std::floor(x));
	const float fx = x - static_cast<float>(ix);
	std::array<float, 4> knots {};
	for (int i = -1; i <= 2; ++i)
	{
		knots[static_cast<size_t>(i + 1)] = Lattice(ix + i);
	}
	return Spline(fx, 4, knots.data());
}
