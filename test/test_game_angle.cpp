/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The GUtils angle family (Common/GUtilsAngle: the arctangent, sine and cosine tables, LHArcTan, the angles from
// positions, the conversions between game, 3D and Scawen angles, the routines from an angle to a position and the
// difference / direction). The tables are checked
// against sums and entries of the original game's tables; the LHArcTan values come from a port run on that table;
// the float values are worked out with the original's 24-bit arithmetic (the cosine rounded to 64 bits).

#include <cmath>

#include <bit>
#include <limits>
#include <numbers>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"

namespace gu = openblack::gutils;
namespace mc = openblack::map_coords;

TEST(GameAngle, Constants)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(gu::k_GameAngleTo3D), 0x3B490FDBu);
	EXPECT_EQ(std::bit_cast<uint32_t>(gu::k_Angle3DToGame), 0x43A2F983u);
	EXPECT_EQ(std::bit_cast<uint32_t>(gu::k_HalfGameAngleTo3D), 0x3AC90FDBu);
	EXPECT_EQ(std::bit_cast<uint32_t>(gu::k_ScawenOffset), 0x3FC90FDBu);
}

TEST(GameAngle, ArcTanTable)
{
	// 257 u16 from the original: the sum, a weighted sum and some entries
	const auto& table = gu::ArcTanTable();
	int64_t sum = 0;
	int64_t weighted = 0;
	for (size_t i = 0; i < table.size(); ++i)
	{
		sum += table[i];
		weighted += static_cast<int64_t>(i + 1) * table[i];
	}
	EXPECT_EQ(sum, 36621);
	EXPECT_EQ(weighted, 6149975);
	EXPECT_EQ(table[0], 0);
	EXPECT_EQ(table[1], 1);
	EXPECT_EQ(table[128], 151);
	EXPECT_EQ(table[200], 216);
	EXPECT_EQ(table[255], 255);
	EXPECT_EQ(table[256], 256);
}

TEST(GameAngle, SinCosTables)
{
	// 2560 i32 from the original: the sums and some entries; COS = SIN + 512
	const auto& table = gu::SinTable();
	int64_t sum = 0;
	int64_t weighted = 0;
	for (size_t i = 0; i < table.size(); ++i)
	{
		sum += table[i];
		if (i < 2048)
		{
			weighted += static_cast<int64_t>(i + 1) * table[i];
		}
	}
	EXPECT_EQ(sum, 21328386);
	EXPECT_EQ(weighted, -43747643392LL);
	EXPECT_EQ(table[1], 201);
	EXPECT_EQ(table[100], 19792);
	EXPECT_EQ(table[256], 46340);
	EXPECT_EQ(table[511], 65535);
	EXPECT_EQ(table[512], 65536);
	EXPECT_EQ(table[1000], 4821);
	EXPECT_EQ(table[1024], 0);
	EXPECT_EQ(table[1500], -65136);
	EXPECT_EQ(table[2047], -201);
	EXPECT_EQ(table[2559], 65535);
	EXPECT_EQ(gu::Sin(0x200), 65536);
	EXPECT_EQ(gu::Cos(0), 65536);
	EXPECT_EQ(gu::Cos(0x400), -65536);
	EXPECT_EQ(gu::Cos(0x200), 0);
	for (uint16_t a = 0; a < 2048; ++a)
	{
		EXPECT_EQ(gu::Cos(a), table[a + 512u]) << a;
	}
}

TEST(GameAngle, ArcTanTableMatches)
{
	// the axes and the diagonals
	EXPECT_EQ(gu::LHArcTan(0, 0), 0);
	EXPECT_EQ(gu::LHArcTan(1, 0), 0);
	EXPECT_EQ(gu::LHArcTan(0, 1), 0x200);
	EXPECT_EQ(gu::LHArcTan(-1, 0), 0x400);
	EXPECT_EQ(gu::LHArcTan(0, -1), 0x600);
	EXPECT_EQ(gu::LHArcTan(1, 1), 0x100);
	EXPECT_EQ(gu::LHArcTan(-1, 1), 0x300);
	EXPECT_EQ(gu::LHArcTan(-1, -1), 0x500);
	EXPECT_EQ(gu::LHArcTan(1, -1), 0x700);
	// one in each octant (a port of the original on its table)
	EXPECT_EQ(gu::LHArcTan(100, 37), 0x72);
	EXPECT_EQ(gu::LHArcTan(-5000, 123456), 0x20C);
	EXPECT_EQ(gu::LHArcTan(7, -3), 0x77D);
	EXPECT_EQ(gu::LHArcTan(-1, -2), 0x569);
	EXPECT_EQ(gu::LHArcTan(65536, 6553), 0x1F);
	// the shift by 8 keeps the low 32 bits: at 2^24 + 1 on both axes the index wraps to 0 (0x200, not the diagonal 0x100)
	EXPECT_EQ(gu::LHArcTan(0x1000001, 0x1000001), 0x200);
	// at most 2.27 steps from atan2
	for (int32_t dx = -300; dx <= 300; dx += 7)
	{
		for (int32_t dz = -300; dz <= 300; dz += 11)
		{
			if (dx == 0 && dz == 0)
			{
				continue;
			}
			const double exact =
			    std::atan2(static_cast<double>(dz), static_cast<double>(dx)) * 2048.0 / (2.0 * std::numbers::pi);
			double d = static_cast<double>(gu::LHArcTan(dx, dz)) - exact;
			d -= 2048.0 * std::round(d / 2048.0);
			EXPECT_LE(std::abs(d), 2.27) << dx << ", " << dz;
		}
	}
}

TEST(GameAngle, AnglesFromPositions)
{
	const mc::MapCoords a {0x10000, 0x20000, 0.0f};
	const mc::MapCoords b {0x10000 + 100, 0x20000 + 37, 5.0f};
	EXPECT_EQ(gu::GetAngleFromXZ(a, b), 0x72);
	EXPECT_EQ(gu::GetAngleFromXZ(glm::ivec2(0x10000, 0x20000), glm::ivec2(0x10000 + 100, 0x20000 + 37)), 0x72);
	EXPECT_FLOAT_EQ(gu::Get3DAngleFromXZ(a, b), static_cast<float>(0x72) * gu::k_GameAngleTo3D);
	// the metre points become MapCoords one by one (ToFixed), and then the difference
	EXPECT_EQ(gu::GetAngleFromXZ(glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 2.0f)), 0x200);
	EXPECT_EQ(gu::GetAngleFromXZ(glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f)), 0x400);
	// quantised: the 3D angle is a multiple of 2 pi / 2048
	const float angle = gu::Get3DAngleFromXZ(glm::vec2(0.0f), glm::vec2(3.0f, 1.0f));
	EXPECT_EQ(angle, gu::ConvertGameAngleTo3D(gu::GetAngleFromXZ(glm::vec2(0.0f), glm::vec2(3.0f, 1.0f))));
}

TEST(GameAngle, Conversions)
{
	EXPECT_EQ(gu::ConvertAngle3DToGame(-0.5f), 1886u); // ftol(-162.97) = -162 & 0x7FF
	EXPECT_EQ(gu::ConvertAngle3DToGame(1.0f), 325u);
	EXPECT_EQ(gu::ConvertAngle3DToGame(3.1415927f), 1024u);
	EXPECT_EQ(gu::ConvertAngle3DToGame(6.2831855f), 0u);                              // 2048.0001 & 0x7FF
	EXPECT_EQ(gu::ConvertAngle3DToGame(std::numeric_limits<float>::quiet_NaN()), 0u); // 0x80000000 & 0x7FF
	EXPECT_EQ(gu::ConvertGameAngleTo3D(0x800 + 3), gu::ConvertGameAngleTo3D(3));      // & 0x7FF
	EXPECT_EQ(gu::ConvertGameAngleTo3D(0x200), static_cast<float>(0x200) * gu::k_GameAngleTo3D);
	// truncated on the way back: 365 of the 2048 angles come back one less (15 is the first)
	EXPECT_EQ(gu::ConvertAngle3DToGame(gu::ConvertGameAngleTo3D(15)), 14u);
	EXPECT_EQ(gu::ConvertAngle3DToGame(gu::ConvertGameAngleTo3D(14)), 14u);
	int lost = 0;
	for (int32_t a = 0; a < 2048; ++a)
	{
		lost += gu::ConvertAngle3DToGame(gu::ConvertGameAngleTo3D(a)) != static_cast<uint32_t>(a) ? 1 : 0;
	}
	EXPECT_EQ(lost, 365);
	// Scawen = 3D + pi / 2, no mask on the way there
	EXPECT_EQ(std::bit_cast<uint32_t>(gu::ConvertGameAngleToScawenAngle(0)), 0x3FC90FDBu);
	EXPECT_EQ(std::bit_cast<uint32_t>(gu::ConvertGameAngleToScawenAngle(0x200)), 0x40490FDBu);
	EXPECT_EQ(std::bit_cast<uint32_t>(gu::ConvertGameAngleToScawenAngle(0x7FF)), 0x40FB3AB0u);
	EXPECT_EQ(std::bit_cast<uint32_t>(gu::ConvertGameAngleToScawenAngle(0x800)), 0x40FB53D2u);
	EXPECT_EQ(gu::ConvertScawenAngleToGameAngle(gu::ConvertGameAngleToScawenAngle(0x200)), 0x200u);
	EXPECT_EQ(gu::ConvertScawenAngleToGameAngle(gu::ConvertGameAngleToScawenAngle(0x7FF)), 0x7FFu);
}

TEST(GameAngle, FromGameAngle)
{
	// the product keeps 32 bits, then >> 16
	EXPECT_EQ(gu::GetXByAngle(0, 0x100), 0x100);
	EXPECT_EQ(gu::GetXByAngle(0, 0x8000), -0x8000); // 65536 x 0x8000 = 2^31 -> INT_MIN >> 16
	EXPECT_EQ(gu::GetZByAngle(0x600, 0x100), -0x100);
	EXPECT_FLOAT_EQ(gu::GetXByAngle(0, 2.0f), 2.0f);
	EXPECT_FLOAT_EQ(gu::GetZByAngle(0x400, 2.0f), 0.0f);
	// arithmetic shifts, so a negative whole goes towards -infinity
	EXPECT_EQ(gu::StepFromAngle(0x400, 0x100), glm::ivec2(-0x100, 0));
	EXPECT_EQ(gu::StepFromAngle(0, -17), glm::ivec2(-32, 0)); // -17 >> 4 = -2
	EXPECT_EQ(gu::StepFromAngle(0x200, 0x10000), glm::ivec2(0, 0x10000));
	EXPECT_EQ(gu::StepFromAngleCoarse(0, 0x10000), glm::ivec2(0x10000, 0));
	EXPECT_EQ(gu::StepFromAngleCoarse(0, -1), glm::ivec2(-256, 0)); // -1 >> 8 = -1
	// ftol(C x (m / 10))
	EXPECT_EQ(gu::GetXByAngleMetersDistance(0, 10.0f), 0x10000);
	EXPECT_EQ(gu::GetZByAngleMetersDistance(0x600, 5.0f), -0x8000);
	// CalcRandomPos's call (an angle, whole distance 10) is (0, 0): 10 >> 4 = 0
	EXPECT_EQ(gu::GetPosFromGameAngle(0x123, 10), mc::MapCoords {});
	// whole = ftol(1 / 10 x 65536) = 6553, >> 4 = 409, x 65536 >> 12 = 6544: the low 4 bits are lost
	EXPECT_EQ(gu::GetPosFromGameAngle(0, 1.0f), (mc::MapCoords {6544, 0, 0.0f}));
}

TEST(GameAngle, FromAngle)
{
	EXPECT_EQ(gu::GetPosFromAngle(0.0f, 10.0f), (mc::MapCoords {0x10000, 0, 0.0f}));
	EXPECT_EQ(gu::GetPosFromAngle(3.1415927f, 10.0f), (mc::MapCoords {-0x10000, 0, 0.0f}));
	EXPECT_EQ(gu::GetPosFromAngle(1.5707964f, 10.0f).z, 0x10000);
	EXPECT_EQ(gu::GetPosFromAngle(0.5f, 7.25f).x, 41697);
	// one rounding of the extended cosine times m: a cosf() would give -198360 here
	EXPECT_EQ(gu::GetPosFromAngle(std::bit_cast<float>(0x406EB37Fu), std::bit_cast<float>(0x42118497u)).x, -198359);
	EXPECT_EQ(gu::GetPosFromAngle(std::bit_cast<float>(0x3EABB91Du), std::bit_cast<float>(0x4212C7ECu)).x, 227085);
	// the position's metres plus cos(a) m, back with GUtils' x 65536 / 10; the altitude stays
	mc::MapCoords p {0x10000, 0x20000, 3.0f};
	gu::AddDistanceFromAngle(p, 0.0f, 5.0f);
	EXPECT_EQ(p, (mc::MapCoords {0x18000, 0x20000, 3.0f}));
	EXPECT_EQ(gu::GetPointFromAngle(0.0f, 3.0f), glm::vec3(3.0f, 0.0f, 0.0f));
}

TEST(GameAngle, DifferenceAndDirection)
{
	EXPECT_EQ(gu::GetAngleDifference(0, 0), 0u);
	EXPECT_EQ(gu::GetAngleDifference(0, 0x400), 0x400u);
	EXPECT_EQ(gu::GetAngleDifference(0, 0x401), 0x3FFu);
	EXPECT_EQ(gu::GetAngleDifference(0x7FF, 0), 1u);
	EXPECT_EQ(gu::GetAngleDifference(0x100, 0x700), 0x200u);
	EXPECT_EQ(gu::GetAngleSign(5, 5), 0);
	EXPECT_EQ(gu::GetAngleSign(0, 1), 1);
	EXPECT_EQ(gu::GetAngleSign(1, 0), -1);
	// |d| == 0x400 does not wrap: +0x400 -> +1, -0x400 -> -1
	EXPECT_EQ(gu::GetAngleSign(0, 0x400), 1);
	EXPECT_EQ(gu::GetAngleSign(0x400, 0), -1);
	EXPECT_EQ(gu::GetAngleSign(0x100, 0x500), 1);
	// past it, the short way round
	EXPECT_EQ(gu::GetAngleSign(0, 0x401), -1);
	EXPECT_EQ(gu::GetAngleSign(0x7FF, 0), 1);
	EXPECT_EQ(gu::GetAngleSign(0, 0x7FF), -1);
}

TEST(GameAngle, MapCoordsOperators)
{
	// MapCoords::operator+ / operator-: the altitude added / subtracted too
	const mc::MapCoords a {10, -20, 1.5f};
	const mc::MapCoords b {3, 4, 0.25f};
	EXPECT_EQ(a + b, (mc::MapCoords {13, -16, 1.75f}));
	EXPECT_EQ(a - b, (mc::MapCoords {7, -24, 1.25f}));
}
