/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GUtilsAngle.h"

#include <cmath>

#include "Common/GUtilsDistance.h"

using namespace openblack;
using openblack::map_coords::MapCoords;

namespace
{
namespace map_coords = openblack::map_coords;

/// The tables are static data in the original; the double formulas give every entry (checked against a dump of both
/// tables, see test_game_angle.cpp)
constexpr double k_TwoPi = 6.283185307179586476925;

/// The sine / cosine of a float angle, extended in the original: the double value here, rounded to a float only by
/// the product with the distance (one rounding, as in the original)
float CosTimes(float radians, float metres)
{
	return static_cast<float>(std::cos(static_cast<double>(radians)) * static_cast<double>(metres));
}
float SinTimes(float radians, float metres)
{
	return static_cast<float>(std::sin(static_cast<double>(radians)) * static_cast<double>(metres));
}

using map_coords::FtoL;
} // namespace

const std::array<uint16_t, 257>& gutils::ArcTanTable()
{
	static const auto k_Table = [] {
		std::array<uint16_t, 257> table {};
		for (size_t i = 0; i < table.size(); ++i)
		{
			// trunc(atan(i / 256) x 2048 / 2 pi)
			table[i] = static_cast<uint16_t>(std::trunc(std::atan(static_cast<double>(i) / 256.0) * 2048.0 / k_TwoPi));
		}
		return table;
	}();
	return k_Table;
}

const std::array<int32_t, 2560>& gutils::SinTable()
{
	static const auto k_Table = [] {
		std::array<int32_t, 2560> table {};
		for (size_t i = 0; i < table.size(); ++i)
		{
			// trunc(65536 sin(i x 2 pi / 2048)), the dividing by 2048 exact
			table[i] = static_cast<int32_t>(std::trunc(65536.0 * std::sin(static_cast<double>(i) * k_TwoPi / 2048.0)));
		}
		return table;
	}();
	return k_Table;
}

int32_t gutils::Cos(uint16_t angle)
{
	// COS[a] = SIN[a + 512]
	return SinTable()[static_cast<size_t>(angle & k_GameAngleMask) + 512];
}

int32_t gutils::Sin(uint16_t angle)
{
	return SinTable()[static_cast<size_t>(angle & k_GameAngleMask)];
}

uint16_t gutils::LHArcTan(int32_t dx, int32_t dz)
{
	const auto& table = ArcTanTable();
	// an unsigned divide; the shift keeps the low 32 bits
	const auto t = [&table](int32_t num, int32_t den) {
		return static_cast<int32_t>(table[(static_cast<uint32_t>(num) << 8u) / static_cast<uint32_t>(den)]);
	};
	const int32_t x = -dx;
	const int32_t z = dz;
	if (z == 0 && x == 0)
	{
		return 0;
	}
	int32_t a;
	if (z >= 0)
	{
		if (x >= 0)
		{
			a = z >= x ? 0x200 + t(x, z) : 0x400 - t(z, x);
		}
		else
		{
			a = z >= -x ? 0x200 - t(-x, z) : t(z, -x);
		}
	}
	else
	{
		if (x >= 0)
		{
			a = -z >= x ? 0x600 - t(x, -z) : 0x400 + t(-z, x);
		}
		else
		{
			a = -z >= -x ? 0x600 + t(-x, -z) : 0x800 - t(-z, -x);
		}
	}
	return static_cast<uint16_t>(a & k_GameAngleMask);
}

uint16_t gutils::GetAngleFromDXDZ(int32_t dx, int32_t dz)
{
	return LHArcTan(dx, dz); // & 0xFFFF
}

uint16_t gutils::GetAngleFromXZ(const MapCoords& from, const MapCoords& to)
{
	return GetAngleFromDXDZ(to.x - from.x, to.z - from.z);
}

uint16_t gutils::GetAngleFromXZ(glm::ivec2 from, glm::ivec2 to)
{
	return GetAngleFromDXDZ(to.x - from.x, to.y - from.y);
}

uint16_t gutils::GetAngleFromXZ(glm::vec2 from, glm::vec2 to)
{
	return GetAngleFromXZ(map_coords::FromMetres(from), map_coords::FromMetres(to));
}

float gutils::Get3DAngleFromXZ(const MapCoords& from, const MapCoords& to)
{
	return ConvertGameAngleTo3D(GetAngleFromXZ(from, to));
}

float gutils::Get3DAngleFromXZ(glm::ivec2 from, glm::ivec2 to)
{
	return ConvertGameAngleTo3D(GetAngleFromXZ(from, to));
}

float gutils::Get3DAngleFromXZ(glm::vec2 from, glm::vec2 to)
{
	return ConvertGameAngleTo3D(GetAngleFromXZ(from, to));
}

uint32_t gutils::ConvertAngle3DToGame(float radians)
{
	const float scaled = radians * k_Angle3DToGame;
	// truncated towards 0, then & 0x7FF: a negative value wraps
	return static_cast<uint32_t>(FtoL(scaled)) & static_cast<uint32_t>(k_GameAngleMask);
}

float gutils::ConvertGameAngleTo3D(int32_t angle)
{
	// & 0x7FF, converted exactly, one product
	return static_cast<float>(angle & k_GameAngleMask) * k_GameAngleTo3D;
}

uint32_t gutils::ConvertScawenAngleToGameAngle(float radians)
{
	const float shifted = radians - k_ScawenOffset; // rounded to a float
	return ConvertAngle3DToGame(shifted);
}

float gutils::ConvertGameAngleToScawenAngle(uint16_t angle)
{
	// & 0xFFFF, << 1, converted exactly, then a product and a sum: two roundings
	const float scaled = static_cast<float>(static_cast<int32_t>(angle) << 1) * k_HalfGameAngleTo3D;
	return scaled + k_ScawenOffset;
}

int32_t gutils::GetXByAngle(uint16_t angle, int32_t distance)
{
	// the low 32 bits of the product, arithmetic shift by 16
	return static_cast<int32_t>(static_cast<uint32_t>(Cos(angle)) * static_cast<uint32_t>(distance)) >> 16;
}

int32_t gutils::GetZByAngle(uint16_t angle, int32_t distance)
{
	return static_cast<int32_t>(static_cast<uint32_t>(Sin(angle)) * static_cast<uint32_t>(distance)) >> 16;
}

float gutils::GetXByAngle(uint16_t angle, float distance)
{
	const float scaled = static_cast<float>(Cos(angle)) * distance;
	return scaled * (1.0f / 65536.0f);
}

float gutils::GetZByAngle(uint16_t angle, float distance)
{
	const float scaled = static_cast<float>(Sin(angle)) * distance;
	return scaled * (1.0f / 65536.0f);
}

glm::ivec2 gutils::StepFromAngle(uint16_t angle, int32_t whole)
{
	// arithmetic shifts by 4 and 12 around a 32-bit product
	const int32_t s = whole >> 4;
	const auto mul = [s](int32_t table) {
		return static_cast<int32_t>(static_cast<uint32_t>(table) * static_cast<uint32_t>(s)) >> 12;
	};
	return {mul(Cos(angle)), mul(Sin(angle))};
}

glm::ivec2 gutils::StepFromAngleCoarse(uint16_t angle, int32_t whole)
{
	// arithmetic shifts by 8 and 8 around a 32-bit product
	const int32_t s = whole >> 8;
	const auto mul = [s](int32_t table) {
		return static_cast<int32_t>(static_cast<uint32_t>(table) * static_cast<uint32_t>(s)) >> 8;
	};
	return {mul(Cos(angle)), mul(Sin(angle))};
}

int32_t gutils::GetXByAngleMetersDistance(uint16_t angle, float metres)
{
	const float cells = metres / k_MetresPerCell;
	return FtoL(static_cast<float>(Cos(angle)) * cells);
}

int32_t gutils::GetZByAngleMetersDistance(uint16_t angle, float metres)
{
	const float cells = metres / k_MetresPerCell;
	return FtoL(static_cast<float>(Sin(angle)) * cells);
}

MapCoords gutils::GetPosFromGameAngle(uint16_t angle, int32_t whole)
{
	const auto step = StepFromAngle(angle, whole);
	return {step.x, step.y, 0.0f};
}

MapCoords gutils::GetPosFromGameAngle(uint16_t angle, float metres)
{
	return GetPosFromGameAngle(angle, ConvertMetersToWholeDistance(metres));
}

MapCoords gutils::GetPosFromAngle(float radians, float metres)
{
	// cos(a) x m, x 65536, / 10, truncated; then the same with sin
	return {map_coords::ToFixedGUtils(CosTimes(radians, metres)), map_coords::ToFixedGUtils(SinTimes(radians, metres)), 0.0f};
}

void gutils::AddDistanceFromAngle(MapCoords& pos, float radians, float metres)
{
	// cos(a) x m plus p.x in metres (one rounding), x 65536, / 10, truncated; then z with sin
	const float dx = CosTimes(radians, metres);
	const float x = dx + map_coords::ToMetres(pos.x);
	pos.x = map_coords::ToFixedGUtils(x);
	const float dz = SinTimes(radians, metres);
	const float z = dz + map_coords::ToMetres(pos.z);
	pos.z = map_coords::ToFixedGUtils(z);
}

glm::vec3 gutils::GetPointFromAngle(float radians, float metres)
{
	return {CosTimes(radians, metres), 0.0f, SinTimes(radians, metres)};
}

uint32_t gutils::GetAngleDifference(int32_t a, int32_t b)
{
	// |a - b|, the far way round above 0x400 (unsigned compare)
	const int32_t d = a - b;
	const auto magnitude = static_cast<uint32_t>(d < 0 ? -d : d);
	return magnitude > 0x400u ? 0x800u - magnitude : magnitude;
}

int32_t gutils::GetAngleSign(int32_t from, int32_t to)
{
	int32_t d = to - from;
	if (d == 0)
	{
		return 0;
	}
	const auto magnitude = static_cast<uint32_t>(d < 0 ? -d : d);
	if (magnitude > 0x400u) // unsigned compare
	{
		d += d < 0 ? 0x800 : -0x800;
	}
	return d < 0 ? -1 : 1;
}
