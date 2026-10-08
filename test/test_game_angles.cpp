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

#include <bit>
#include <limits>
#include <utility>

#include <3D/ObjectMatrix.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

// affine::ArcTanOctant / GetYAngle / WrapAngle (3D/ObjectMatrix.h) against the original. The goldens were made by
// emulating the original's x87 arithmetic at 24 bits (divisions, additions and subtractions rounded to 24 bits, the
// arctangent to 64) on an exact arctangent, then rounded to a float as the callers store it; each is compared bit for
// bit.

namespace affine = openblack::affine;

namespace
{
constexpr float k_NaN = std::numeric_limits<float>::quiet_NaN();

struct ArcTanCase
{
	float a;
	float b;
	float expected;
	int branch; ///< 1: a >= |b|, 2: b >= |a|, 3: a <= -|b|, 4: b <= -|a| (the four exits)
};

struct YAngleCase
{
	float x;
	float y;
	float z;
	float expected;
	int branch; ///< 0: x x + z z <= 1e-6, else ArcTanOctant's
};

struct WrapCase
{
	float a;
	float expected;
};

struct PoseCase
{
	float x;
	float z;
	float expected;
};

void ExpectBits(float actual, float expected)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(actual), std::bit_cast<uint32_t>(expected)) << actual << " vs " << expected;
}
} // namespace

TEST(GameAngles, ArcTanOctant)
{
	// ArcTanOctant(a, b) = atan2(b, a): the axes (signed zeros included), the ties, one case per octant, odd values
	const ArcTanCase cases[] = {
	    {1.0f, 0.0f, 0.0f, 1},                             // 0x00000000, atan2 0
	    {0.0f, 1.0f, 1.57079637f, 2},                      // 0x3FC90FDB, atan2 1.57079633
	    {-1.0f, 0.0f, 3.14159274f, 3},                     // 0x40490FDB, atan2 3.14159265
	    {0.0f, -1.0f, -1.57079637f, 4},                    // 0xBFC90FDB, atan2 -1.57079633
	    {-1.0f, -0.0f, 3.14159274f, 3},                    // 0x40490FDB, atan2 -3.14159265
	    {-0.0f, 1.0f, 1.57079637f, 2},                     // 0x3FC90FDB, atan2 1.57079633
	    {-0.0f, -1.0f, -1.57079637f, 4},                   // 0xBFC90FDB, atan2 -1.57079633
	    {2.5f, -0.0f, -0.0f, 1},                           // 0x80000000, atan2 -0
	    {1.0f, 1.0f, 0.785398185f, 1},                     // 0x3F490FDB, atan2 0.785398163
	    {1.0f, -1.0f, -0.785398185f, 1},                   // 0xBF490FDB, atan2 -0.785398163
	    {-1.0f, 1.0f, 2.3561945f, 2},                      // 0x4016CBE4, atan2 2.35619449
	    {-1.0f, -1.0f, -2.3561945f, 4},                    // 0xC016CBE4, atan2 -2.35619449
	    {3.0f, 1.0f, 0.321750551f, 1},                     // 0x3EA4BC7D, atan2 0.321750554
	    {1.0f, 3.0f, 1.24904585f, 2},                      // 0x3F9FE0BC, atan2 1.24904577
	    {-1.0f, 3.0f, 1.89254689f, 2},                     // 0x3FF23EFA, atan2 1.89254688
	    {-3.0f, 1.0f, 2.8198421f, 3},                      // 0x4034784B, atan2 2.8198421
	    {-3.0f, -1.0f, -2.8198421f, 3},                    // 0xC034784B, atan2 -2.8198421
	    {-1.0f, -3.0f, -1.89254689f, 4},                   // 0xBFF23EFA, atan2 -1.89254688
	    {1.0f, -3.0f, -1.24904585f, 4},                    // 0xBF9FE0BC, atan2 -1.24904577
	    {3.0f, -1.0f, -0.321750551f, 1},                   // 0xBEA4BC7D, atan2 -0.321750554
	    {0.300000012f, 0.699999988f, 1.16590452f, 2},      // 0x3F953C5C, atan2 1.16590452
	    {-0.699999988f, 0.300000012f, 2.73670101f, 3},     // 0x402F261C, atan2 2.73670085
	    {-0.300000012f, -0.699999988f, -1.97568822f, 4},   // 0xBFFCE35A, atan2 -1.97568813
	    {0.699999988f, -0.300000012f, -0.404891819f, 1},   // 0xBECF4DFB, atan2 -0.404891807
	    {-2.5f, 0.00100000005f, 3.14119267f, 3},           // 0x4049094D, atan2 3.14119265
	    {-2.5f, -0.00100000005f, -3.14119267f, 3},         // 0xC049094D, atan2 -3.14119265
	    {12345.6777f, 0.100000001f, 8.10000074e-06f, 1},   // 0x3707E53D, atan2 8.10000096e-06
	    {9.99999968e-21f, 2.9999999e-20f, 1.24904585f, 2}, // 0x3F9FE0BC, atan2 1.24904577
	    {-0.123456702f, 0.987654328f, 1.69515133f, 2},     // 0x3FD8FAB8, atan2 1.69515123
	};
	for (const auto& c : cases)
	{
		SCOPED_TRACE(testing::Message() << "a " << c.a << " b " << c.b << " branch " << c.branch);
		ExpectBits(static_cast<float>(affine::ArcTanOctant(c.a, c.b)), c.expected);
	}
	// (0, 0): the first branch's 0 / 0
	EXPECT_TRUE(std::isnan(affine::ArcTanOctant(0.0f, 0.0f)));
}

TEST(GameAngles, ArcTanOctantBranchesTwoToFourAreFloats)
{
	// the 24-bit subtractions and additions leave a float value on the stack
	const std::pair<float, float> pairs[] = {{0.3f, 0.7f}, {-0.7f, 0.3f}, {-0.3f, -0.7f}, {-2.5f, 0.001f}};
	for (const auto& [a, b] : pairs)
	{
		const double v = affine::ArcTanOctant(a, b);
		EXPECT_EQ(v, static_cast<double>(static_cast<float>(v))) << a << " " << b;
	}
}

TEST(GameAngles, GetYAngle)
{
	// GetYAngle(v) = ArcTanOctant(-z, x) = atan2(x, -z); 0 when x x + z z <= 1e-6. 0.001 squared is
	// 0x358637BE, one bit over the threshold; x = 0x3A83126E, z = 0x34A10FB0 sum to 0x358637BD itself (0); y is never read
	const YAngleCase cases[] = {
	    {1.0f, 5.0f, 0.0f, 1.57079637f, 2},                      // 0x3FC90FDB, sum 0x3F800000
	    {0.0f, 0.0f, -1.0f, 0.0f, 1},                            // 0x00000000, sum 0x3F800000
	    {0.0f, 0.0f, 1.0f, 3.14159274f, 3},                      // 0x40490FDB, sum 0x3F800000
	    {-1.0f, 0.0f, 0.0f, -1.57079637f, 4},                    // 0xBFC90FDB, sum 0x3F800000
	    {0.300000012f, 5.0f, -0.400000006f, 0.643501103f, 1},    // 0x3F24BC7D, sum 0x3E800000
	    {-0.600000024f, -2.0f, 0.800000012f, -2.4980917f, 3},    // 0xC01FE0BC, sum 0x3F800000
	    {0.600000024f, 0.0f, 0.800000012f, 2.4980917f, 3},       // 0x401FE0BC, sum 0x3F800000
	    {-0.600000024f, 0.0f, -0.800000012f, -0.643501103f, 1},  // 0xBF24BC7D, sum 0x3F800000
	    {0.00100000005f, 0.0f, 0.0f, 1.57079637f, 2},            // 0x3FC90FDB, sum 0x358637BE
	    {0.000699999975f, 9.0f, 0.000699999975f, 0.0f, 0},       // 0x00000000, sum 0x3583888B
	    {0.00079999998f, 0.0f, 0.000699999975f, 2.28962636f, 2}, // 0x4012893D, sum 0x3597AA80
	    {0.0f, 1.0f, 0.0f, 0.0f, 0},                             // 0x00000000, sum 0x00000000
	    {9.99999975e-05f, 0.0f, -9.99999975e-05f, 0.0f, 0},      // 0x00000000, sum 0x32ABCC76
	    {0.000999999931f, 0.0f, 3.00000011e-07f, 0.0f, 0},       // 0x00000000, sum 0x358637BD (exactly the threshold: <=)
	};
	for (const auto& c : cases)
	{
		SCOPED_TRACE(testing::Message() << "v " << c.x << " " << c.y << " " << c.z << " branch " << c.branch);
		ExpectBits(static_cast<float>(affine::GetYAngle(glm::vec3(c.x, c.y, c.z))), c.expected);
	}
}

TEST(GameAngles, WrapAngle)
{
	// WrapAngle: a > pi -> a - 2 pi; a < -pi -> a + 2 pi; once only; +-pi stay, their next floats wrap
	const WrapCase cases[] = {
	    {0.0f, 0.0f},                 // 0x00000000 -> 0x00000000
	    {3.14159274f, 3.14159274f},   // 0x40490FDB -> 0x40490FDB
	    {3.14159298f, -3.1415925f},   // 0x40490FDC -> 0xC0490FDA
	    {-3.14159274f, -3.14159274f}, // 0xC0490FDB -> 0xC0490FDB
	    {-3.14159298f, 3.1415925f},   // 0xC0490FDC -> 0x40490FDA
	    {4.0f, -2.28318548f},         // 0x40800000 -> 0xC0121FB6
	    {7.0f, 0.716814518f},         // 0x40E00000 -> 0x3F378128
	    {-7.0f, -0.716814518f},       // 0xC0E00000 -> 0xBF378128
	    {10.0f, 3.71681452f},         // 0x41200000 -> 0x406DE04A
	    {-10.0f, -3.71681452f},       // 0xC1200000 -> 0xC06DE04A
	    {1.0f, 1.0f},                 // 0x3F800000 -> 0x3F800000
	    {-3.0f, -3.0f},               // 0xC0400000 -> 0xC0400000
	    {6.28318548f, 0.0f},          // 0x40C90FDB -> 0x00000000
	};
	for (const auto& c : cases)
	{
		SCOPED_TRACE(testing::Message() << "a " << c.a);
		ExpectBits(affine::WrapAngle(c.a), c.expected);
	}
	EXPECT_TRUE(std::isnan(affine::WrapAngle(k_NaN)));
}

TEST(GameAngles, VillagerEndPhysicsHeading)
{
	// The villager's EndPhysics heading: GetYAngle(row), plus pi rounded to 24 bits, stored as a float, then
	// WrapAngle. Along +x it is 3 pi / 2 rounded, then less 2 pi: one bit off -pi / 2
	const PoseCase cases[] = {
	    {0.0f, -1.0f, 3.14159274f},                  // 0x40490FDB
	    {0.0f, 1.0f, 0.0f},                          // 0x00000000
	    {1.0f, 0.0f, -1.57079649f},                  // 0xBFC90FDC
	    {-1.0f, 0.0f, 1.57079637f},                  // 0x3FC90FDB
	    {0.600000024f, 0.800000012f, -0.643500805f}, // 0xBF24BC78
	    {-0.300000012f, -0.400000006f, 2.4980917f},  // 0x401FE0BC
	    {0.0f, 0.0f, 3.14159274f},                   // 0x40490FDB
	};
	for (const auto& c : cases)
	{
		SCOPED_TRACE(testing::Message() << "x " << c.x << " z " << c.z);
		const auto sum = static_cast<float>(affine::GetYAngle(glm::vec3(c.x, 0.0f, c.z)) + 3.1415927410125732);
		ExpectBits(affine::WrapAngle(sum), c.expected);
	}
}
